#include "DBCParser.h"
#include "CANDatabaseException.h"
#include <string>
#include <iostream>
#include <algorithm>
#include <iterator>
#include "ParsingUtils.h"

using namespace DBCParser;

CANSignal                  parseSignal(Tokenizer& tokenizer);
CANFrame                   parseFrame(Tokenizer& tokenizer);
std::set<std::string>      parseECUs(Tokenizer& tokenizer);
void                       parseNewSymbols(Tokenizer& tokenizer);
void                       addBADirective(Tokenizer& tokenizer,
                                          CANDatabase& db);
void                       addComment(Tokenizer& tokenizer, CANDatabase& db);
void                       parseSignalChoices(Tokenizer& tokenizer,
                                           CANDatabase& db);

static std::string VERSION_TOKEN = "VERSION";
static std::string NS_SECTION_TOKEN = "NS_";
static std::string BIT_TIMING_TOKEN = "BS_";
static std::string NODE_DEF_TOKEN = "BU_";
static std::string MESSAGE_DEF_TOKEN = "BO_";
static std::string SIG_DEF_TOKEN = "SG_";
static std::string SIG_VAL_DEF_TOKEN = "VAL_";
static std::string ENV_VAR_TOKEN = "EV_";
static std::string COMMENT_TOKEN = "CM_";
static std::string ATTR_DEF_TOKEN = "BA_DEF_";
static std::string ATTR_DEF_DEFAULT_TOKEN = "BA_DEF_DEF_";
static std::string ATTR_VAL_TOKEN = "BA_";

// Duplicates but I don't think it demands so much memory
// anyway...
static std::set<std::string> SUPPORTED_DBC_TOKENS = {
  VERSION_TOKEN, BIT_TIMING_TOKEN, NODE_DEF_TOKEN, MESSAGE_DEF_TOKEN,
  SIG_DEF_TOKEN, SIG_VAL_DEF_TOKEN, ENV_VAR_TOKEN, COMMENT_TOKEN,
  ATTR_DEF_TOKEN, ATTR_DEF_DEFAULT_TOKEN, ATTR_VAL_TOKEN
};

static std::set<std::string> NS_TOKENS = {
  "CM_", "BA_DEF_", "BA_", "VAL_", "CAT_DEF_", "CAT_", "FILTER", "BA_DEF_DEF_",
  "EV_DATA_", "ENVVAR_DATA", "SGTYPE_", "SGTYPE_VAL_", "BA_DEF_SGTYPE_", "BA_SGTYPE_",
  "SIG_TYPE_DEF_"
};

static std::set<std::string> UNSUPPORTED_DBC_TOKENS = {
  "VAL_TABLE_", "BO_TX_BU_", "ENVVAR_DATA_",
  "SGTYPE_", "SIG_GROUP_"
}; 

bool is_dbc_token(const Token& token) {
  return SUPPORTED_DBC_TOKENS.count(token.image) > 0 ||
         NS_TOKENS.count(token.image) > 0||
         UNSUPPORTED_DBC_TOKENS.count(token.image) > 0;
}

void addBADirective(Tokenizer& tokenizer, CANDatabase& db) {
  Token infoType;
  assert_current_token(tokenizer, "BA_");

  infoType = assert_token(tokenizer, Token::StringLiteral);
  if(infoType == "GenMsgCycleTime" || infoType == "CycleTime") {
    assert_token(tokenizer, "BO_");
    Token frameId = assert_token(tokenizer, Token::Number);
    Token period = assert_token(tokenizer, Token::Number);
    assert_token(tokenizer, ";");

    if(period == Token::NegativeNumber) {
      warning("cannot set negative period",
              tokenizer.lineCount());
      return;
    }
    else if(frameId == Token::NegativeNumber) {
      warning("invalid frame id",
              tokenizer.lineCount());
      return;
    }

    unsigned int iFrameId = frameId.toUInt();
    unsigned int iPeriod = period.toUInt();

    try {
      db.at(iFrameId).setPeriod(iPeriod);
    }
    catch (const std::out_of_range& e) {
      std::string tempStr = std::to_string(iFrameId) + " does not exist at line " + std::to_string(tokenizer.lineCount());
      throw CANDatabaseException(tempStr);
    }
  }
  else {
    std::cout << "WARNING: Unrecognized BA_ command " << infoType.image
              << " at line " << tokenizer.lineCount()
              << std::endl;
    tokenizer.skipUntil(";");
  }
}

CANDatabase DBCParser::fromTokenizer(Tokenizer& tokenizer) {
  return fromTokenizer("", tokenizer);
}

std::string parseVersionSection(Tokenizer& tokenizer) {
  if(peek_token(tokenizer, VERSION_TOKEN)) {
    Token candb_version = assert_token(tokenizer, Token::StringLiteral);
    // std::cout << "CANdb++ version: " << candb_version.image << std::endl;
    return candb_version.image;
  }

  return "";
}

static void
parseNSSection(Tokenizer& tokenizer) {
  if(!peek_token(tokenizer, NS_SECTION_TOKEN))
    return;

  assert_token(tokenizer, ":");
  
  Token token = tokenizer.getNextToken();
  while (NS_TOKENS.count(token.image) > 0) {
    token = tokenizer.getNextToken();
  }

  tokenizer.saveTokenIfNotEof(token);
}

static void
parseBitTimingSection(Tokenizer& tokenizer) {
  assert_token(tokenizer, BIT_TIMING_TOKEN);
  assert_token(tokenizer, ":");

  if (peek_token(tokenizer, Token::PositiveNumber)) {
    Token baudrate = assert_current_token(tokenizer, Token::PositiveNumber);
    assert_token(tokenizer, ":");
    Token btr1 = assert_token(tokenizer, Token::PositiveNumber);
    assert_token(tokenizer, ",");
    Token btr2 = assert_token(tokenizer, Token::PositiveNumber);
  }
}

static void
parseNodesSection(Tokenizer& tokenizer, CANDatabase& db) {
  assert_token(tokenizer, NODE_DEF_TOKEN);
  assert_token(tokenizer, ":");

  std::set<std::string> nodes;

  if(!peek_token(tokenizer, Token::Identifier)) {
    return;
  }

  Token currentToken = assert_token(tokenizer, Token::Identifier);

  // Looking for all the identifiers on the same line
  while(currentToken != Token::Eof &&
        !is_dbc_token(currentToken)) {
    
    nodes.insert(currentToken.image);
    currentToken = assert_token(tokenizer, Token::Identifier);
  }

  tokenizer.saveTokenIfNotEof(currentToken);
}

static void
parseUnsupportedCommandSection(Tokenizer& tokenizer, const std::string& command) {
  while(peek_token(tokenizer, command)) {
    // In DBC files, some instructions don't finish by a semi-colon.
    // Fotunately, all the unsupported ones do finish by a semi-colon.
    warning("Skipped \"" + command + "\" instruction because it is not supported", tokenizer.lineCount()); 
    tokenizer.skipUntil(";");
  }
}

static void
parseSigDefInstruction(Tokenizer& tokenizer, CANFrame& frame) {
  assert_current_token(tokenizer, SIG_DEF_TOKEN);


  Token name = assert_token(tokenizer, Token::Identifier);
  assert_token(tokenizer, ":");
  Token startBit = assert_token(tokenizer, Token::PositiveNumber);
  assert_token(tokenizer, "|");
  Token length = assert_token(tokenizer, Token::PositiveNumber);
  assert_token(tokenizer, "@");
  Token endianess = assert_token(tokenizer, Token::PositiveNumber);
  Token signedness = assert_token(tokenizer, Token::ArithmeticSign);
  assert_token(tokenizer, "(");
  Token scale = assert_token(tokenizer, Token::Number);
  assert_token(tokenizer, ",");
  Token offset = assert_token(tokenizer, Token::Number);
  assert_token(tokenizer, ")");
  assert_token(tokenizer, "[");
  Token min = assert_token(tokenizer, Token::Number);
  assert_token(tokenizer, "|");
  Token max = assert_token(tokenizer, Token::Number);
  assert_token(tokenizer, "]");
  Token unit = assert_token(tokenizer, Token::StringLiteral);
   
  // ECU are ignored for now
  Token targetECU = assert_token(tokenizer, Token::Identifier);  
  while (peek_token(tokenizer, ",")) {
    targetECU = assert_token(tokenizer, Token::Identifier);
  }

  frame.addSignal(
    CANSignal(
      name.image,
      startBit.toUInt(),
      length.toUInt(),
      scale.toDouble(),
      offset.toDouble(),
      signedness == "-" ? CANSignal::Signed : CANSignal::Unsigned,
      endianess == "0" ? CANSignal::BigEndian : CANSignal::LittleEndian,
      CANSignal::Range::fromString(min.image, max.image)
    )
  );
}

static void
parseMsgDefSection(Tokenizer& tokenizer, CANDatabase& db) {
  while(peek_token(tokenizer, MESSAGE_DEF_TOKEN)) {
    Token id = assert_token(tokenizer, Token::PositiveNumber);
    Token name = assert_token(tokenizer, Token::Identifier);

    assert_token(tokenizer, ":");

    Token dlc = assert_token(tokenizer, Token::PositiveNumber);
    Token ecu = assert_token(tokenizer, Token::Identifier);

    CANFrame new_frame(
      name.image, id.toUInt(), dlc.toUInt());

    while(peek_token(tokenizer, SIG_DEF_TOKEN)) {
      parseSigDefInstruction(tokenizer, new_frame);
    }

    db.addFrame(new_frame);
  }
}

static void
parseMsgCommentInstruction(Tokenizer& tokenizer, CANDatabase& db) {
  Token targetFrame = assert_token(tokenizer, Token::PositiveNumber);
  Token comment = assert_token(tokenizer, Token::StringLiteral);
  assert_token(tokenizer, ";");

  auto frame_id = targetFrame.toUInt();
  if(db.contains(frame_id)) {
    db.at(frame_id).setComment(comment.image);
  }
  else {
    warning("Invalid comment instruction: Frame with id " + targetFrame.image + " does not exist", tokenizer.lineCount());
  }
}

static void
parseSigCommentInstruction(Tokenizer& tokenizer, CANDatabase& db) {
  Token targetFrame = assert_token(tokenizer, Token::PositiveNumber);
  Token targetSignal = assert_token(tokenizer, Token::Identifier);
  Token comment = assert_token(tokenizer, Token::StringLiteral);
  assert_token(tokenizer, ";");

  if(!db.contains(targetFrame.toUInt())) {
    warning("Invalid comment instruction: Frame with id " + targetFrame.image + " does not exist", tokenizer.lineCount());
    return;
  }

  CANFrame& frame = db[targetFrame.toUInt()];
  if(!frame.contains(targetSignal.image)) {
    warning("Invalid comment instruction: Frame with id " + targetFrame.image + " does not have "
            "a signal named \"" + targetSignal.image + "\"", tokenizer.lineCount());
  }
  else {
    frame[targetSignal.image].setComment(comment.image);
  }

}

static void
parseCommentSection(Tokenizer& tokenizer, CANDatabase& db) {
  while(peek_token(tokenizer, COMMENT_TOKEN)) {
    if(peek_token(tokenizer, Token::StringLiteral)) {
      // TODO: handle global comment
      assert_token(tokenizer, ";");
      warning("Unsupported comment instruction", tokenizer.lineCount());
      continue;
    }

    Token commentType = assert_token(tokenizer, Token::Identifier);
    if(commentType == MESSAGE_DEF_TOKEN) {
      parseMsgCommentInstruction(tokenizer, db);
    }
    else if(commentType == SIG_DEF_TOKEN) {
      parseSigCommentInstruction(tokenizer, db);
    }
    else {
      warning("Unsupported comment instruction", tokenizer.lineCount());
      tokenizer.skipUntil(";");
    }
  }
}

static void
parseAttrValSection(Tokenizer& tokenizer, CANDatabase& db) {
  while(peek_token(tokenizer, ATTR_VAL_TOKEN)) {
    Token attrType = assert_token(tokenizer, Token::StringLiteral);

    if(attrType != "GenMsgCycleTime" && attrType != "CycleTime") {
      tokenizer.skipUntil(";");
      warning("Unsupported BA_ operation", tokenizer.lineCount());
      continue;
    }
 
    assert_token(tokenizer, "BO_");
    Token frameId = assert_token(tokenizer, Token::PositiveNumber);
    Token period = assert_token(tokenizer, Token::PositiveNumber);
    assert_token(tokenizer, ";");

    try {
      db[frameId.toUInt()].setPeriod(period.toUInt());
    }
    catch (const std::out_of_range& e) {
     warning(frameId.image + " does not exist", tokenizer.lineCount());
    }
  }
}

static void
parseValDescSection(Tokenizer& tokenizer, CANDatabase& db) {
  while(peek_token(tokenizer, SIG_VAL_DEF_TOKEN)) {
    Token targetFrame = assert_token(tokenizer, Token::PositiveNumber);
    Token targetSignal = assert_token(tokenizer, Token::Identifier);
    
    std::map<unsigned int, std::string> targetChoices;

    while(!peek_token(tokenizer, ";")) {
      Token value = assert_token(tokenizer, Token::Number);
      Token desc = assert_token(tokenizer, Token::StringLiteral);

      targetChoices.insert(std::make_pair(value.toUInt(), desc.image));
    }

    if(!db.contains(targetFrame.toUInt())) {
      warning("Invalid VAL_ instruction: Frame with id " + targetFrame.image + " does not exist", tokenizer.lineCount());
      continue;
    }

    CANFrame& frame = db[targetFrame.toUInt()];
    if(!frame.contains(targetSignal.image)) {
      warning("Invalid VAL_ instruction: Frame " + targetFrame.image + " does not have "
              "a signal named \"" + targetSignal.image + "\"", tokenizer.lineCount());
    }
    else {
      frame[targetSignal.image].setChoices(targetChoices);
    }
  }
}


CANDatabase DBCParser::fromTokenizer(const std::string& name, Tokenizer& tokenizer) {
  CANDatabase result(name);

  parseVersionSection(tokenizer);
  parseNSSection(tokenizer);
  parseBitTimingSection(tokenizer);
  parseNodesSection(tokenizer, result);
  parseUnsupportedCommandSection(tokenizer, "VAL_TABLE_");
  parseMsgDefSection(tokenizer, result);
  parseUnsupportedCommandSection(tokenizer, "BO_TX_BU_");
  parseUnsupportedCommandSection(tokenizer, "EV_");
  parseUnsupportedCommandSection(tokenizer, "SGTYPE_");
  parseCommentSection(tokenizer, result);
  parseUnsupportedCommandSection(tokenizer, "BA_DEF_");
  parseUnsupportedCommandSection(tokenizer, "SIG_VALTYPE_");
  parseUnsupportedCommandSection(tokenizer, "BA_DEF_DEF_");
  parseAttrValSection(tokenizer, result);
  parseValDescSection(tokenizer, result);

  while(!is_token(tokenizer, Token::Eof)) {
    warning("Unexpected token " + tokenizer.getCurrentToken().image + 
            " at line " + std::to_string(tokenizer.lineCount())     +
            " (maybe is it an unsupported instruction ? maybe is it a misplaced instruction ?)",
            tokenizer.lineCount());
    tokenizer.skipUntil(";");
  }

  return result;
}
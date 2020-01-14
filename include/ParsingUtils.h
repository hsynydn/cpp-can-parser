#ifndef ParsingUtils_H
#define ParsingUtils_H

#include "Tokenizer.h"
#include <string>
#include <iostream>
#include "CANDatabaseException.h"

void throw_error(const std::string& category, const std::string& description,
                unsigned long long line);

void warning(const std::string& description, unsigned long long line) ;

const Token&
assert_token(Tokenizer& tokenizer, const std::string& token);

const Token&
assert_token(Tokenizer& tokenizer, Token::Type targetType);

const Token&
assert_current_token(const Tokenizer& tokenizer, const std::string& token);

const Token&
assert_current_token(const Tokenizer& tokenizer, Token::Type type);

bool is_current_token(const Tokenizer& tokenizer, const std::string& token);

bool is_current_token(const Tokenizer& tokenizer, Token::Type token);

bool is_token(Tokenizer& tokenizer, const std::string& token);

bool is_token(Tokenizer& tokenizer, Token::Type token);

bool peek_token(Tokenizer& tokenizer, Token::Type type);

bool peek_token(Tokenizer& tokenizer, const std::string& token);

#endif

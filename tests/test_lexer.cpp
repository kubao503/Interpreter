#include <gtest/gtest.h>

#include "lexer.hpp"
#include "lexer_errors.hpp"
#include "types.hpp"

using TypeSequence = std::vector<Token::Type>;

class LexerTest : public testing::Test {
   protected:
    void Init(const std::string& input) {
        stream_ = std::istringstream(input);
        source_ = std::make_unique<Source>(stream_);
        lexer_ = std::make_unique<Lexer>(*source_);
    }

    std::istringstream stream_;
    std::unique_ptr<Source> source_;
    std::unique_ptr<Lexer> lexer_;
};

TEST_F(LexerTest, getToken_true) {
    Init("true");

    auto token = lexer_->getToken();
    ASSERT_EQ(token.getType(), Token::Type::TRUE_CONST) << "Invalid type";
    ASSERT_TRUE(std::holds_alternative<bool>(token.getValue())) << "Invalid value";
    ASSERT_TRUE(std::get<bool>(token.getValue()));

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_false) {
    Init("false");

    auto token = lexer_->getToken();
    ASSERT_EQ(token.getType(), Token::Type::FALSE_CONST) << "Invalid type";
    ASSERT_TRUE(std::holds_alternative<bool>(token.getValue())) << "Invalid value";
    ASSERT_FALSE(std::get<bool>(token.getValue()));

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_while_keyword) {
    Init("while");

    auto token = lexer_->getToken();

    ASSERT_EQ(token.getType(), Token::Type::WHILE_KW) << "Invalid type";
    EXPECT_TRUE(std::holds_alternative<std::monostate>(token.getValue()))
        << "Invalid value";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_id) {
    Init("valid_identifier_123");

    auto token = lexer_->getToken();

    ASSERT_EQ(token.getType(), Token::Type::ID) << "Invalid type";
    EXPECT_EQ(std::get<std::string>(token.getValue()), "valid_identifier_123")
        << "Invalid value";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_id_pretending_to_be_keyword) {
    Init("While");

    auto token = lexer_->getToken();

    ASSERT_EQ(token.getType(), Token::Type::ID) << "Keywords are lowercase only";
    EXPECT_EQ(std::get<std::string>(token.getValue()), "While") << "Invalid value";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_int) {
    Init("1234");

    auto token = lexer_->getToken();

    ASSERT_EQ(token.getType(), Token::Type::INT_CONST) << "Invalid type";
    EXPECT_EQ(std::get<Integral>(token.getValue()), 1234) << "Invalid value";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_int_with_leading_zero) {
    Init("01234");

    ASSERT_EQ(std::get<Integral>(lexer_->getToken().getValue()), 0)
        << "First part of int";

    EXPECT_EQ(std::get<Integral>(lexer_->getToken().getValue()), 1234)
        << "Second part of int";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_int_max) {
    Init("2147483647");

    EXPECT_NO_THROW(lexer_->getToken()) << "Just at the max";
}

TEST_F(LexerTest, getToken_int_overflow) {
    Init("2147483648");

    EXPECT_THROW(lexer_->getToken(), NumericOverflow) << "Max exceeded";
}

TEST_F(LexerTest, getToken_float) {
    Init("12.125");

    auto token = lexer_->getToken();

    ASSERT_EQ(token.getType(), Token::Type::FLOAT_CONST) << "Invalid type";
    EXPECT_EQ(std::get<float>(token.getValue()), 12.125f) << "Invalid value";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

class LexerFloatTest : public LexerTest,
                       public testing::WithParamInterface<std::string> {};

TEST_P(LexerFloatTest, getToken_invalid_float) {
    Init(GetParam());

    EXPECT_THROW(lexer_->getToken(), InvalidFloat);
}

INSTANTIATE_TEST_SUITE_P(InvalidFloat, LexerFloatTest, testing::Values("1..125", "1."));

TEST_F(LexerTest, getToken_float_max) {
    Init("0.2147483647");

    EXPECT_NO_THROW(lexer_->getToken()) << "Just at the max";
}

TEST_F(LexerTest, getToken_float_overflow) {
    Init("0.2147483648");

    EXPECT_THROW(lexer_->getToken(), NumericOverflow) << "Max exceeded";
}

TEST_F(LexerTest, getToken_invalid) {
    Init("&324");

    EXPECT_THROW(lexer_->getToken(), InvalidToken);
}

TEST_F(LexerTest, getToken_empty_source) {
    Init("");

    for (int i{0}; i < 5; ++i) {
        auto token = lexer_->getToken();
        ASSERT_EQ(token.getType(), Token::Type::ETX) << "Invalid type";
        EXPECT_TRUE(std::holds_alternative<std::monostate>(token.getValue()))
            << "Invalid value";
    }
}

TEST_F(LexerTest, getToken_leading_white_space) {
    Init("   \t \n  true");

    auto token = lexer_->getToken();

    EXPECT_EQ(token.getType(), Token::Type::TRUE_CONST) << "Invalid type";
}

class LexerOperatorTest
    : public LexerTest,
      public testing::WithParamInterface<std::pair<std::string, Token::Type>> {};

TEST_P(LexerOperatorTest, getToken_operators) {
    auto& [op, tokenType] = GetParam();
    Init(op);

    auto token = lexer_->getToken();
    EXPECT_EQ(token.getType(), tokenType) << "Invalid type";
    EXPECT_TRUE(std::holds_alternative<std::monostate>(token.getValue()))
        << "Invalid value";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

auto operatorPairs = testing::Values(
    std::make_pair("<", Token::Type::LT_OP), std::make_pair("<=", Token::Type::LTE_OP),
    std::make_pair(">", Token::Type::GT_OP), std::make_pair(">=", Token::Type::GTE_OP),
    std::make_pair("=", Token::Type::ASGN_OP), std::make_pair("==", Token::Type::EQ_OP),
    std::make_pair("!=", Token::Type::NEQ_OP), std::make_pair(";", Token::Type::SEMI),
    std::make_pair(",", Token::Type::CMA), std::make_pair(".", Token::Type::DOT),
    std::make_pair("+", Token::Type::ADD_OP), std::make_pair("-", Token::Type::MIN_OP),
    std::make_pair("*", Token::Type::MULT_OP), std::make_pair("/", Token::Type::DIV_OP),
    std::make_pair("(", Token::Type::L_PAR), std::make_pair(")", Token::Type::R_PAR),
    std::make_pair("{", Token::Type::L_C_BR), std::make_pair("}", Token::Type::R_C_BR));

INSTANTIATE_TEST_SUITE_P(Operators, LexerOperatorTest, operatorPairs);

TEST_F(LexerTest, getToken_invalid_not_equal_operator) {
    Init("!");

    EXPECT_THROW(lexer_->getToken(), InvalidToken);
}

TEST_F(LexerTest, getToken_str_const_empty) {
    Init(R"("")");

    auto token = lexer_->getToken();
    ASSERT_EQ(token.getType(), Token::Type::STR_CONST) << "Invalid type";
    EXPECT_EQ(std::get<std::string>(token.getValue()), "") << "Invalid token.getValue()";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_str_const_new_line) {
    Init(R"("a\nb")");

    auto token = lexer_->getToken();
    ASSERT_EQ(token.getType(), Token::Type::STR_CONST) << "Invalid type";
    EXPECT_EQ(std::get<std::string>(token.getValue()), "a\nb")
        << "Invalid token.getValue()";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_str_const_quotation_mark) {
    Init(R"("\"")");

    auto token = lexer_->getToken();
    ASSERT_EQ(token.getType(), Token::Type::STR_CONST) << "Invalid type";
    EXPECT_EQ(std::get<std::string>(token.getValue()), R"(")")
        << "Invalid token.getValue()";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_str_const_backslash) {
    Init(R"("\\")");

    auto token = lexer_->getToken();
    ASSERT_EQ(token.getType(), Token::Type::STR_CONST) << "Invalid type";
    EXPECT_EQ(std::get<std::string>(token.getValue()), R"(\)")
        << "Invalid token.getValue()";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_str_const_complicated) {
    Init(R"("\"lama \nma \\ delfina\"")");

    auto token = lexer_->getToken();
    ASSERT_EQ(token.getType(), Token::Type::STR_CONST) << "Invalid type";
    EXPECT_EQ(std::get<std::string>(token.getValue()), "\"lama \nma \\ delfina\"")
        << "Invalid token.getValue()";

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_not_terminated_str_const) {
    Init(R"("no ending quotation mark)");

    EXPECT_THROW(lexer_->getToken(), NotTerminatedStrConst)
        << "Str const without ending quotation mark";
}

TEST_F(LexerTest, getToken_backslash_at_the_end_of_file) {
    Init(R"("abc\)");

    EXPECT_THROW(lexer_->getToken(), NotTerminatedStrConst)
        << "Non-terminated string is more important than backslash";
}

TEST_F(LexerTest, getToken_escaping_wrong_char) {
    Init(R"("\a")");

    EXPECT_THROW(lexer_->getToken(), NonEscapableChar) << "cannot escape char 'a'";
}

TEST_F(LexerTest, getToken_comment) {
    Init(R"(# int 12 # "abc")");

    auto token = lexer_->getToken();
    ASSERT_EQ(token.getType(), Token::Type::CMT) << "Invalid type";
    EXPECT_EQ(std::get<std::string>(token.getValue()), R"( int 12 # "abc")");

    EXPECT_EQ(lexer_->getToken().getType(), Token::Type::ETX) << "Invalid type";
}

TEST_F(LexerTest, getToken_end_comment_at_new_line) {
    Init("# first line\n second line");

    auto token = lexer_->getToken();
    ASSERT_EQ(token.getType(), Token::Type::CMT) << "Invalid type";
    EXPECT_EQ(std::get<std::string>(token.getValue()), R"( first line)");
}

TEST_F(LexerTest, getToken_token_position_one_line) {
    Init("int void");

    auto token = lexer_->getToken();
    EXPECT_EQ(token.getPosition().line, 1);
    EXPECT_EQ(token.getPosition().column, 1);

    token = lexer_->getToken();
    EXPECT_EQ(token.getPosition().line, 1);
    EXPECT_EQ(token.getPosition().column, 5);
}

TEST_F(LexerTest, getToken_token_position_two_lines) {
    Init("abc\ndef");

    auto token = lexer_->getToken();
    EXPECT_EQ(token.getPosition().line, 1);
    EXPECT_EQ(token.getPosition().column, 1);

    token = lexer_->getToken();
    EXPECT_EQ(token.getPosition().line, 2);
    EXPECT_EQ(token.getPosition().column, 1);
}

TEST_F(LexerTest, getToken_multiple_tokens) {
    std::string input{
        "void add_one_ref(ref int num) {"
        "    num = num + 1;"
        "}"};
    Init(input);

    TypeSequence seq{
        Token::Type::VOID_KW,   Token::Type::ID,     Token::Type::L_PAR,
        Token::Type::REF_KW,    Token::Type::INT_KW, Token::Type::ID,
        Token::Type::R_PAR,     Token::Type::L_C_BR, Token::Type::ID,
        Token::Type::ASGN_OP,   Token::Type::ID,     Token::Type::ADD_OP,
        Token::Type::INT_CONST, Token::Type::SEMI,   Token::Type::R_C_BR,
        Token::Type::ETX,
    };

    for (auto type : seq)
        EXPECT_EQ(lexer_->getToken().getType(), type) << "Invalid type";
}

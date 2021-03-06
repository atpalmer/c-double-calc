#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "error.h"
#include "syswrap.h"
#include "lexer.h"
#include "parser.h"

void parser_setlinevar(Parser *this, double value) {
    varmap_setval(&this->varmap, "_", value);
}

Parser *parser_new(const char *program) {
    Parser *new = malloc_or_die(sizeof *new);
    new->lexer = lexer_new(program);
    new->curr = NULL;
    new->varmap = NULL;
    return new;
}

void parser_set_buff(Parser *this, const char *program) {
    lexer_free(this->lexer);
    this->lexer = lexer_new(program);
}

void parser_free(Parser *this) {
    lexer_free(this->lexer);
    token_free(this->curr);
    varmap_free(this->varmap);
    free(this);
}

Token *parser_curr(Parser *this) {
    if(!this->curr)
        this->curr = lexer_next(this->lexer);
    return this->curr;
}

void parser_next(Parser *this) {
    if(this->curr)
        token_free(this->curr);
    this->curr = lexer_next(this->lexer);
}

void parser_accept(Parser *this, TokenType token_type) {
    Token *curr = parser_curr(this);
    token_ensure_type(curr, token_type);
    parser_next(this);
}

double parser_expr(Parser *this);

double parser_handle_assignment(Parser *this, const char *key) {
    parser_accept(this, TOKT_EQ);
    double result = parser_expr(this);
    varmap_setval(&this->varmap, key, result);
    return result;
}

double parser_handle_variable(Parser *this) {
    Token *var = parser_curr(this);
    char *key = strdup_or_die(token_varname(var));
    parser_accept(this, TOKT_IDENTIFIER);

    Token *eq = parser_curr(this);

    double result = token_type(eq) == TOKT_EQ
        ? parser_handle_assignment(this, key)
        : varmap_getval(this->varmap, key);

    free(key);
    return result;
}

double parser_paren_expr(Parser *this) {
    parser_accept(this, TOKT_LPAREN);
    double result = parser_expr(this);
    parser_accept(this, TOKT_RPAREN);
    return result;
}

double parser_number(Parser *this) {
    Token *curr = parser_curr(this);
    double result = token_number(curr);
    parser_accept(this, TOKT_NUMBER);
    return result;
}

double parser_atom(Parser *this) {
    Token *curr = parser_curr(this);

    switch(token_type(curr)) {
    case TOKT_NUMBER:
        return parser_number(this);
    case TOKT_LPAREN:
        return parser_paren_expr(this);
    case TOKT_IDENTIFIER:
        return parser_handle_variable(this);
    default:
        break;
    }

    fprintf(stderr, "TokenType Error: Cannot parse atom. Position: %d. Found: '%c' (%d).\n",
        this->lexer->pos, token_type(curr), token_type(curr));
    exit(-1);
}

double parser_signed_atom(Parser *this) {
    Token *curr = parser_curr(this);
    switch(token_type(curr)) {
    case TOKT_MINUS:
        parser_accept(this, TOKT_MINUS);
        return -parser_atom(this);
    case TOKT_PLUS:
        parser_accept(this, TOKT_PLUS);
        return +parser_atom(this);
    default:
        break;
    }

    return parser_atom(this);
}

double parser_factor(Parser *this) {
    double base = parser_signed_atom(this);

    Token *curr = parser_curr(this);
    switch(token_type(curr)) {
    case TOKT_DUBSTAR:
        parser_accept(this, TOKT_DUBSTAR);
        return pow(base, parser_factor(this));
    default:
        break;
    }

    return base;
}

double parser_term(Parser *this) {
    double result = parser_factor(this);

    for(;;) {
        Token *curr = parser_curr(this);

        switch(token_type(curr)) {
        case TOKT_STAR:
            parser_accept(this, TOKT_STAR);
            result *= parser_factor(this);
            break;
        case TOKT_SLASH:
            parser_accept(this, TOKT_SLASH);
            result /= parser_factor(this);
            break;
        case TOKT_DUBSLASH:
            parser_accept(this, TOKT_DUBSLASH);
            result /= parser_factor(this);
            result = floor(result);
            break;
        case TOKT_PERCENT:
            parser_accept(this, TOKT_PERCENT);
            result = fmod(result, parser_factor(this));
            break;
        default:
            goto done;
        }
    }

done:
    return result;
}

double parser_sum(Parser *this) {
    double result = parser_term(this);

    for(;;) {
        Token *curr = parser_curr(this);

        switch(token_type(curr)) {
        case TOKT_PLUS:
            parser_accept(this, TOKT_PLUS);
            result += parser_term(this);
            break;
        case TOKT_MINUS:
            parser_accept(this, TOKT_MINUS);
            result -= parser_term(this);
            break;
        default:
            goto done;
        }
    }

done:
    return result;
}

double parser_expr(Parser *this) {
    double result = parser_sum(this);

    for(;;) {
        Token *curr = parser_curr(this);

        switch(token_type(curr)) {
        case TOKT_DUBEQ:
            parser_accept(this, TOKT_DUBEQ);
            result = (result == parser_sum(this));
            break;
        case TOKT_NE:
            parser_accept(this, TOKT_NE);
            result = (result != parser_sum(this));
            break;
        case TOKT_LT:
            parser_accept(this, TOKT_LT);
            result = (result < parser_sum(this));
            break;
        case TOKT_GT:
            parser_accept(this, TOKT_GT);
            result = (result > parser_sum(this));
            break;
        default:
            goto done;
        }
    }

done:
    return result;
}

double parser_line(Parser *this) {
    double result = parser_expr(this);
    parser_accept(this, TOKT_NEWLINE);
    return result;
}

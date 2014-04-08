%option yylineno
%option warn nodefault
%option nointeractive full
%option noyywrap nounput
%option outfile="c11_lexer.c" header-file="c11_lexer.h"

%e  1019
%p  2807
%n  371
%k  284
%a  1213
%o  1117

O   [0-7]
D   [0-9]
NZ  [1-9]
L   [a-zA-Z_]
A   [a-zA-Z_0-9]
H   [a-fA-F0-9]
HP  (0[xX])
E   ([Ee][+-]?{D}+)
P   ([Pp][+-]?{D}+)
FS  (f|F|l|L)
IS  (((u|U)(l|L|ll|LL)?)|((l|L|ll|LL)(u|U)?))
CP  (u|U|L)
SP  (u8|u|U|L)
ES  (\\(['"\?\\abfnrtv]|[0-7]{1,3}|x[a-fA-F0-9]+))
WS  [ \t\v\n\f]

%{
#include "frontend.h"
#include <stdio.h>
#include "c11_parser.h"

static void comment(void);
static void consume_logical_line (void);
static int check_type(void);
static void store_lval (void);

#define YY_USER_ACTION store_lval();

%}

%%
"/*"                                    { comment(); }
"//".*                                    { /* consume //-comment */ }
"#"                                     { consume_logical_line (); }

"auto"                  { return(AUTO); }
"break"                 { return(BREAK); }
"case"                  { return(CASE); }
"char"                  { return(CHAR); }
"const"                 { return(CONST); }
"continue"              { return(CONTINUE); }
"default"               { return(DEFAULT); }
"do"                    { return(DO); }
"double"                { return(DOUBLE); }
"else"                  { return(ELSE); }
"enum"                  { return(ENUM); }
"extern"                { return(EXTERN); }
"float"                 { return(FLOAT); }
"for"                   { return(FOR); }
"goto"                  { return(GOTO); }
"if"                    { return(IF); }
"inline"                { return(INLINE); }
"int"                   { return(INT); }
"long"                  { return(LONG); }
"register"              { return(REGISTER); }
"restrict"              { return(RESTRICT); }
"return"                { return(RETURN); }
"short"                 { return(SHORT); }
"signed"                { return(SIGNED); }
"sizeof"                { return(SIZEOF); }
"static"                { return(STATIC); }
"struct"                { return(STRUCT); }
"switch"                { return(SWITCH); }
"typedef"               { return(TYPEDEF); }
"union"                 { return(UNION); }
"unsigned"              { return(UNSIGNED); }
"void"                  { return(VOID); }
"volatile"              { return(VOLATILE); }
"while"                 { return(WHILE); }
"_Alignas"                              { return ALIGNAS; }
"_Alignof"                              { return ALIGNOF; }
"_Atomic"                               { return ATOMIC; }
"_Bool"                                 { return BOOL; }
"_Complex"                              { return COMPLEX; }
"_Generic"                              { return GENERIC; }
"_Imaginary"                            { return IMAGINARY; }
"_Noreturn"                             { return NORETURN; }
"_Static_assert"                        { return STATIC_ASSERT; }
"_Thread_local"                         { return THREAD_LOCAL; }
"__func__"                              { return FUNC_NAME; }

"__attribute__"                         { return GCCATTRIBUTE; }
"__asm__"                               { return GCCASM; }
"__inline"                              { return INLINE; }
"__restrict"                            { return RESTRICT; }
"__extension__"                         {}

{L}{A}*                 { return check_type(); }

{HP}{H}+{IS}?               { return I_CONSTANT; }
{NZ}{D}*{IS}?               { return I_CONSTANT; }
"0"{O}*{IS}?                { return I_CONSTANT; }
{CP}?"'"([^'\\\n]|{ES})+"'"     { return I_CONSTANT; }

{D}+{E}{FS}?                { return F_CONSTANT; }
{D}*"."{D}+{E}?{FS}?            { return F_CONSTANT; }
{D}+"."{E}?{FS}?            { return F_CONSTANT; }
{HP}{H}+{P}{FS}?            { return F_CONSTANT; }
{HP}{H}*"."{H}+{P}{FS}?         { return F_CONSTANT; }
{HP}{H}+"."{P}{FS}?         { return F_CONSTANT; }

({SP}?\"([^"\\\n]|{ES})*\"{WS}*)+   { return STRING_LITERAL; }

"..."                   { return ELLIPSIS; }
">>="                   { return RIGHT_ASSIGN; }
"<<="                   { return LEFT_ASSIGN; }
"+="                    { return ADD_ASSIGN; }
"-="                    { return SUB_ASSIGN; }
"*="                    { return MUL_ASSIGN; }
"/="                    { return DIV_ASSIGN; }
"%="                    { return MOD_ASSIGN; }
"&="                    { return AND_ASSIGN; }
"^="                    { return XOR_ASSIGN; }
"|="                    { return OR_ASSIGN; }
">>"                    { return RIGHT_OP; }
"<<"                    { return LEFT_OP; }
"++"                    { return INC_OP; }
"--"                    { return DEC_OP; }
"->"                    { return PTR_OP; }
"&&"                    { return AND_OP; }
"||"                    { return OR_OP; }
"<="                    { return LE_OP; }
">="                    { return GE_OP; }
"=="                    { return EQ_OP; }
"!="                    { return NE_OP; }
";"                 { return ';'; }
("{"|"<%")              { return '{'; }
("}"|"%>")              { return '}'; }
","                 { return ','; }
":"                 { return ':'; }
"="                 { return '='; }
"("                 { return '('; }
")"                 { return ')'; }
("["|"<:")              { return '['; }
("]"|":>")              { return ']'; }
"."                 { return '.'; }
"&"                 { return '&'; }
"!"                 { return '!'; }
"~"                 { return '~'; }
"-"                 { return '-'; }
"+"                 { return '+'; }
"*"                 { return '*'; }
"/"                 { return '/'; }
"%"                 { return '%'; }
"<"                 { return '<'; }
">"                 { return '>'; }
"^"                 { return '^'; }
"|"                 { return '|'; }
"?"                 { return '?'; }

{WS}                    { /* whitespace separates tokens */ }
.                   { /* discard bad characters */ }

%%

static void comment(void)
{
    int c;

    while ((c = input()) != 0)
        if (c == '*')
        {
            while ((c = input()) == '*')
                ;

            if (c == '/')
                return;

            if (c == 0)
                break;
        }
    yyerror("unterminated comment");
}

static void consume_logical_line (void)
{
  int c, prev = 0;
  while (((c = input ()) != 0) && ((c != '\n') && (prev != '\\')))
    prev = c;
}

static int check_type(void)
{
    bool sns = struct_ns;
    struct_ns = false;
    if (sns)
      return IDENTIFIER;

    switch (sym_type(yytext))
    {
    case TYPEDEF_NAME:                /* previously defined */
        return TYPEDEF_NAME;
    case ENUMERATION_CONSTANT:        /* previously defined */
        return ENUMERATION_CONSTANT;
    default:                          /* includes undefined */
        return IDENTIFIER;
    }
}

static void store_lval (void)
{
  if (*yytext != '#')
  {
    yylval = strdup (yytext);
  }
}

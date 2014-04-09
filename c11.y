%token  IDENTIFIER I_CONSTANT F_CONSTANT STRING_LITERAL FUNC_NAME SIZEOF
%token  PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token  AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token  SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token  XOR_ASSIGN OR_ASSIGN
%token  TYPEDEF_NAME ENUMERATION_CONSTANT

%token  TYPEDEF EXTERN STATIC AUTO REGISTER INLINE
%token  CONST RESTRICT VOLATILE
%token  BOOL CHAR SHORT INT LONG SIGNED UNSIGNED FLOAT DOUBLE VOID
%token  COMPLEX IMAGINARY 
%token  STRUCT UNION ENUM ELLIPSIS

%token  CASE DEFAULT IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN

%token  ALIGNAS ALIGNOF ATOMIC GENERIC NORETURN STATIC_ASSERT THREAD_LOCAL

%token  PRAGMA PRAGMA_ARG

/* GCC extension stuff */
%token GCCATTRIBUTE GCCASM

%start translation_unit
%expect 3

%{
#include "frontend.h"
#include "model.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern int yylex (void);

static bool ce;
static int td; // typedef depth (may be negative when in struct/union/paramlist)

static bool gt = true;

#define MKVAL(fmt, args...) \
  char *s; if (asprintf (&s, fmt, ##args) < 0) yyerror ("out of memory");
#define FREE1(a)        free (a)
#define FREE2(a,b)      do { free (a); free (b); } while (0)
#define FREE3(a,b,c)    do { FREE2(a,b); free (c); } while (0)
#define FREE4(a,b,c,d)  do { FREE3(a,b,c); free (d); } while (0)

%}

%%

primary_expression
    : IDENTIFIER
    | constant
    | string
    | '(' expression ')'     { MKVAL("(%s)", $2); $$=s; FREE1($2); }
    | generic_selection
    ;

gccattribute_list
    : gccattribute
    | gccattribute gccattribute_list
    ;

gccattribute
    : GCCATTRIBUTE '(''(' gccattribute_value_list ')'')'
    ;

gccattribute_value_list
    : gccattribute_value
    | gccattribute_value ',' gccattribute_value_list
    ;

gccattribute_value
    : IDENTIFIER
    | IDENTIFIER '(' IDENTIFIER ')'
    | IDENTIFIER '(' gccattribute_arg_list ')'
    ;

gccattribute_arg_list
    : gccattribute_arg
    | gccattribute_arg ',' gccattribute_arg_list
    ;

gccattribute_arg
    : IDENTIFIER
    | constant
    ;

gccasm
    : GCCASM '(' STRING_LITERAL ')'

gccstuff
    : gccattribute_list
    | gccasm
    | gccasm gccattribute_list
    ;

constant
    : I_CONSTANT        /* includes character_constant */
    | F_CONSTANT
    | ENUMERATION_CONSTANT  /* after it has been defined as such */
    ;

enumeration_constant        /* before it has been defined as such */
    : IDENTIFIER
        { add_enum_constant ($1); }
    ;

string
    : STRING_LITERAL
    | FUNC_NAME
    ;

generic_selection
    : GENERIC '(' assignment_expression ',' generic_assoc_list ')'
        { FREE1($3); }
    ;

generic_assoc_list
    : generic_association
    | generic_assoc_list ',' generic_association
    ;

generic_association
    : type_name ':' assignment_expression
        { FREE1($3); }
    | DEFAULT ':' assignment_expression
        { FREE1($3); }
    ;

postfix_expression
    : primary_expression
    | postfix_expression '[' expression ']'
    | postfix_expression '(' ')'
    | postfix_expression '(' argument_expression_list ')'
    | postfix_expression '.' IDENTIFIER
    | postfix_expression PTR_OP IDENTIFIER
    | postfix_expression INC_OP
    | postfix_expression DEC_OP
    | '(' type_name ')' '{' initializer_list '}'
    | '(' type_name ')' '{' initializer_list ',' '}'
    ;

argument_expression_list
    : assignment_expression
    | argument_expression_list ',' assignment_expression
        { FREE1($3); }
    ;

unary_expression
    : postfix_expression
    | INC_OP unary_expression        {$$=strdup ("<n/a>");}
    | DEC_OP unary_expression        {$$=strdup ("<n/a>");}
    | unary_operator cast_expression { MKVAL("%s %s", $1, $2); FREE1($2); }
    | SIZEOF unary_expression    { MKVAL("sizeof %s", $2); $$=s; FREE1($2); }
    | SIZEOF '(' type_name ')'   { MKVAL("sizeof(%s)", $3); $$=s; }
    | ALIGNOF '(' type_name ')'  { MKVAL("alignof(%s)", $3); $$=s; }
    ;

unary_operator
    : '&'
    | '*'
    | '+'
    | '-'
    | '~'
    | '!'
    ;

cast_expression
    : unary_expression
    | '(' type_name ')' cast_expression
        { MKVAL("(%s)%s", $2, $4); $$=s; FREE1($4); }
    ;

multiplicative_expression
    : cast_expression
    | multiplicative_expression '*' cast_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    | multiplicative_expression '/' cast_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    | multiplicative_expression '%' cast_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

additive_expression
    : multiplicative_expression
    | additive_expression '+' multiplicative_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    | additive_expression '-' multiplicative_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

shift_expression
    : additive_expression
    | shift_expression LEFT_OP additive_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    | shift_expression RIGHT_OP additive_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

relational_expression
    : shift_expression
    | relational_expression '<' shift_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    | relational_expression '>' shift_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    | relational_expression LE_OP shift_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    | relational_expression GE_OP shift_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

equality_expression
    : relational_expression
    | equality_expression EQ_OP relational_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    | equality_expression NE_OP relational_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

and_expression
    : equality_expression
    | and_expression '&' equality_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

exclusive_or_expression
    : and_expression
    | exclusive_or_expression '^' and_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

inclusive_or_expression
    : exclusive_or_expression
    | inclusive_or_expression '|' exclusive_or_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

logical_and_expression
    : inclusive_or_expression
    | logical_and_expression AND_OP inclusive_or_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

logical_or_expression
    : logical_and_expression
    | logical_or_expression OR_OP logical_and_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

conditional_expression
    : logical_or_expression
    | logical_or_expression '?' expression ':' conditional_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

assignment_expression
    : conditional_expression
    | unary_expression assignment_operator assignment_expression
        { MKVAL("%s%s%s", $1, $2, $3); $$=s; FREE2($1, $3); }
    ;

assignment_operator
    : '='
    | MUL_ASSIGN
    | DIV_ASSIGN
    | MOD_ASSIGN
    | ADD_ASSIGN
    | SUB_ASSIGN
    | LEFT_ASSIGN
    | RIGHT_ASSIGN
    | AND_ASSIGN
    | XOR_ASSIGN
    | OR_ASSIGN
    ;

expression
    : assignment_expression
    | expression ',' assignment_expression
        { MKVAL("%s,%s", $1, $3); FREE2($1, $3); }
    ;

constant_expression
    : conditional_expression    /* with constraints */
    ;

declaration
    : declaration_specifiers ';' {set_type($1); if (ce) end_capture(NO_MEMBERS); ce=false; reset_info(); gt=true; }
    | declaration_specifiers init_declarator_list ';' {set_type($1); if (ce) end_capture(NO_MEMBERS); ce=false; reset_info(); gt=true; }
    | static_assert_declaration
    ;

declaration_specifiers
    : storage_class_specifier declaration_specifiers {$$=$2;}
    | storage_class_specifier
    | type_specifier declaration_specifiers {MKVAL("%s %s",$1,$2); $$=s;}
    | type_specifier
    | type_qualifier declaration_specifiers {$$=$2;}
    | type_qualifier
    | function_specifier declaration_specifiers
    | function_specifier
    | alignment_specifier declaration_specifiers
    | alignment_specifier
    ;

init_declarator_list
    : init_declarator
    | init_declarator_list ',' init_declarator
    ;

init_declarator
    : declarator '=' initializer
    | declarator
    ;

storage_class_specifier
    : TYPEDEF   /* identifiers must be flagged as TYPEDEF_NAME */
       { ++td; gt=true; capture (NO_MEMBERS); }
    | EXTERN
    | STATIC
    | THREAD_LOCAL
    | AUTO
    | REGISTER
    ;

type_specifier
    : VOID
    | CHAR
    | SHORT
    | INT
    | LONG
    | FLOAT
    | DOUBLE
    | SIGNED
    | UNSIGNED
    | BOOL
    | COMPLEX
    | IMAGINARY    /* non-mandated extension */
    | atomic_type_specifier
    | struct_or_union_specifier
    | enum_specifier {MKVAL("int"); $$=s;}
    | TYPEDEF_NAME
    ;

struct_or_union_specifier
    : struct_or_union '{' {struct_ns=false; capture (WITH_MEMBERS); --td;} struct_declaration_list '}' {++td; $$=0; end_capture (WITH_MEMBERS);}
    | struct_or_union IDENTIFIER {struct_ns=false; add_placeholder($2); capture (WITH_MEMBERS);} '{' {--td;} struct_declaration_list '}' {++td; MKVAL("struct %s",$2); $$=s; set_name (s); end_capture (WITH_MEMBERS);}
    | struct_or_union IDENTIFIER {struct_ns=false; MKVAL("struct %s",$2); add_placeholder(s); $$=s;}
    ;

struct_or_union
    : STRUCT { struct_ns=true; }
    | UNION  { struct_ns=true; }
    ;

struct_declaration_list
    : struct_declaration {capture_member ();}
    | struct_declaration_list struct_declaration {capture_member ();}
    ;

struct_declaration
    : specifier_qualifier_list ';'  {set_type($1);} /* for anonymous struct/union */
    | specifier_qualifier_list pragma ';'  {set_type($1); handle_pragma($2);} /* for anonymous struct/union */
    | specifier_qualifier_list struct_declarator_list ';' {set_type($1);}
    | specifier_qualifier_list struct_declarator_list pragma ';' {set_type($1); handle_pragma($3);}
    | static_assert_declaration
    ;

specifier_qualifier_list
    : type_specifier specifier_qualifier_list {MKVAL("%s %s",$1,$2); $$=s;}
    | type_specifier
    | type_qualifier specifier_qualifier_list {$$=$2;}
    | type_qualifier
    ;

struct_declarator_list
    : struct_declarator
    | struct_declarator_list ',' struct_declarator /* TODO: support this */
    ;

struct_declarator
    : ':' constant_expression
    | declarator ':' constant_expression
    | declarator
    ;

enum_specifier
    : ENUM '{' enumerator_list '}'
    | ENUM '{' enumerator_list ',' '}'
    | ENUM IDENTIFIER '{' enumerator_list '}'
    | ENUM IDENTIFIER '{' enumerator_list ',' '}'
    | ENUM IDENTIFIER
    ;

enumerator_list
    : enumerator
    | enumerator_list ',' enumerator
    ;

enumerator  /* identifiers must be flagged as ENUMERATION_CONSTANT */
    : enumeration_constant '=' constant_expression
    | enumeration_constant
    ;

atomic_type_specifier
    : ATOMIC '(' type_name ')'  { $$ = $3; }
    ;

type_qualifier
    : CONST
    | RESTRICT
    | VOLATILE
    | ATOMIC
    ;

function_specifier
    : INLINE
    | NORETURN
    ;

alignment_specifier
    : ALIGNAS '(' type_name ')'
    | ALIGNAS '(' constant_expression ')'
    ;

declarator
    : pointer direct_declarator
    | pointer direct_declarator gccstuff
    | direct_declarator
    | direct_declarator gccstuff
    ;

direct_declarator
    : IDENTIFIER
        { if (td > 0) { --td; add_typedef_name ($1); ce=true; } set_name ($1); gt=false; }
    | '(' declarator ')'
    | direct_declarator '[' ']'
    | direct_declarator '[' '*' ']'
    | direct_declarator '[' STATIC type_qualifier_list assignment_expression ']'
        { FREE1($5); }
    | direct_declarator '[' STATIC assignment_expression ']'
        { FREE1($4); }
    | direct_declarator '[' type_qualifier_list '*' ']'
    | direct_declarator '[' type_qualifier_list STATIC assignment_expression ']'
        { FREE1($5); }
    | direct_declarator '[' type_qualifier_list assignment_expression ']'
        { FREE1($4); }
    | direct_declarator '[' type_qualifier_list ']'
    | direct_declarator '[' assignment_expression ']'
        { note_array_size ($3); FREE1($3); }
    | direct_declarator '(' {--td;} parameter_type_list ')' {++td;}
    | direct_declarator '(' ')'
    | direct_declarator '(' identifier_list ')'
    ;

pointer
    : '*' type_qualifier_list pointer   { note_pointer (); }
    | '*' type_qualifier_list           { note_pointer (); }
    | '*' pointer                       { note_pointer (); }
    | '*'                               { note_pointer (); }
    ;

type_qualifier_list
    : type_qualifier
    | type_qualifier_list type_qualifier
    ;


parameter_type_list
    : parameter_list ',' ELLIPSIS
    | parameter_list
    ;

parameter_list
    : parameter_declaration
    | parameter_list ',' parameter_declaration
    ;

parameter_declaration
    : declaration_specifiers declarator
    | declaration_specifiers abstract_declarator
    | declaration_specifiers
    ;

identifier_list
    : IDENTIFIER
    | identifier_list ',' IDENTIFIER
    ;

type_name
    : specifier_qualifier_list abstract_declarator
    | specifier_qualifier_list
    ;

abstract_declarator
    : pointer direct_abstract_declarator
    | pointer
    | direct_abstract_declarator
    ;

direct_abstract_declarator
    : '(' abstract_declarator ')'
    | '[' ']'
    | '[' '*' ']'
    | '[' STATIC type_qualifier_list assignment_expression ']'
        { FREE1($4); }
    | '[' STATIC assignment_expression ']'
        { FREE1($3); }
    | '[' type_qualifier_list STATIC assignment_expression ']'
        { FREE1($4); }
    | '[' type_qualifier_list assignment_expression ']'
        { FREE1($3); }
    | '[' type_qualifier_list ']'
    | '[' assignment_expression ']'
        { FREE1($2); }
    | direct_abstract_declarator '[' ']'
    | direct_abstract_declarator '[' '*' ']'
    | direct_abstract_declarator '[' STATIC type_qualifier_list assignment_expression ']'
        { FREE1($5); }
    | direct_abstract_declarator '[' STATIC assignment_expression ']'
        { FREE1($4); }
    | direct_abstract_declarator '[' type_qualifier_list assignment_expression ']'
        { FREE1($4); }
    | direct_abstract_declarator '[' type_qualifier_list STATIC assignment_expression ']'
        { FREE1($5); }
    | direct_abstract_declarator '[' type_qualifier_list ']'
    | direct_abstract_declarator '[' assignment_expression ']'
        { FREE1($3); }
    | '(' ')'
    | '(' parameter_type_list ')'
    | direct_abstract_declarator '(' ')'
    | direct_abstract_declarator '(' parameter_type_list ')'
    ;

initializer
    : '{' initializer_list '}'
    | '{' initializer_list ',' '}'
    | assignment_expression
    ;

initializer_list
    : designation initializer
    | initializer
    | initializer_list ',' designation initializer
    | initializer_list ',' initializer
    ;

designation
    : designator_list '='
    ;

designator_list
    : designator
    | designator_list designator
    ;

designator
    : '[' constant_expression ']'
    | '.' IDENTIFIER
    ;

static_assert_declaration
    : STATIC_ASSERT '(' constant_expression ',' STRING_LITERAL ')' ';'
    ;

statement
    : labeled_statement
    | compound_statement
    | expression_statement
    | selection_statement
    | iteration_statement
    | jump_statement
    ;

labeled_statement
    : IDENTIFIER ':' statement
    | CASE constant_expression ':' statement
    | DEFAULT ':' statement
    ;

compound_statement
    : '{' '}'
    | '{'  block_item_list '}'
    ;

block_item_list
    : block_item
    | block_item_list block_item
    ;

block_item
    : declaration
    | statement
    ;

expression_statement
    : ';'
    | expression ';'
    ;

selection_statement
    : IF '(' expression ')' statement ELSE statement
    | IF '(' expression ')' statement
    | SWITCH '(' expression ')' statement
    ;

iteration_statement
    : WHILE '(' expression ')' statement
    | DO statement WHILE '(' expression ')' ';'
    | FOR '(' expression_statement expression_statement ')' statement
    | FOR '(' expression_statement expression_statement expression ')' statement
    | FOR '(' declaration expression_statement ')' statement
    | FOR '(' declaration expression_statement expression ')' statement
    ;

jump_statement
    : GOTO IDENTIFIER ';'
    | CONTINUE ';'
    | BREAK ';'
    | RETURN ';'
    | RETURN expression ';'
    ;

translation_unit
    : external_declaration
    | translation_unit external_declaration
    ;

external_declaration
    : function_definition
    | declaration
    | pragma
    ;

function_definition
    : declaration_specifiers declarator declaration_list compound_statement
    | declaration_specifiers declarator compound_statement
    ;

declaration_list
    : declaration
    | declaration_list declaration
    ;

pragma
    : PRAGMA '(' STRING_LITERAL ')' { $$=$3; }
    | PRAGMA pragma_arg_list { $$=$2; }

pragma_arg_list
    : PRAGMA_ARG
    | PRAGMA_ARG pragma_arg_list { MKVAL("%s %s",$1,$2); $$=s; }

%%
#include <stdio.h>

extern int yylineno;
extern const char *yytext;

bool struct_ns;

void yyerror(const char *s)
{
    fflush(stdout);
    fprintf(stderr, "error on line %d near '%s': %s\n", yylineno, yytext, s);
    exit (1);
}

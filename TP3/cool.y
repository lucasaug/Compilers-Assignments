/*
 *  cool.y
 *              Parser definition for the COOL language.
 *
 */
%{
#include <iostream>
#include "cool-tree.h"
#include "stringtab.h"
#include "utilities.h"

extern char *curr_filename;

void yyerror(char *s);        /*  defined below; called for each parse error */
extern int yylex();           /*  the entry point to the lexer  */

/************************************************************************/
/*                DONT CHANGE ANYTHING IN THIS SECTION                  */

Program ast_root;	      /* the result of the parse  */
Classes parse_results;        /* for use in semantic analysis */
int omerrs = 0;               /* number of errors in lexing and parsing */
%}

/* A union of all the types that can be the result of parsing actions. */
%union {
  Boolean boolean;
  Symbol symbol;
  Program program;
  Class_ class_;
  Classes classes;
  Feature feature;
  Features features;
  Formal formal;
  Formals formals;
  Case case_;
  Cases cases;
  Expression expression;
  Expressions expressions;
  char *error_msg;
}

/* 
   Declare the terminals; a few have types for associated lexemes.
   The token ERROR is never used in the parser; thus, it is a parse
   error when the lexer returns it.

   The integer following token declaration is the numeric constant used
   to represent that token internally.  Typically, Bison generates these
   on its own, but we give explicit numbers to prevent version parity
   problems (bison 1.25 and earlier start at 258, later versions -- at
   257)
*/
%token CLASS 258 ELSE 259 FI 260 IF 261 IN 262 
%token INHERITS 263 LET 264 LOOP 265 POOL 266 THEN 267 WHILE 268
%token CASE 269 ESAC 270 OF 271 DARROW 272 NEW 273 ISVOID 274
%token <symbol>  STR_CONST 275 INT_CONST 276 
%token <boolean> BOOL_CONST 277
%token <symbol>  TYPEID 278 OBJECTID 279 
%token ASSIGN 280 NOT 281 LE 282 ERROR 283

/*  DON'T CHANGE ANYTHING ABOVE THIS LINE, OR YOUR PARSER WONT WORK       */
/**************************************************************************/
 
   /* Complete the nonterminal list below, giving a type for the semantic
      value of each non terminal. (See section 3.6 in the bison 
      documentation for details). */

/* Declare types for the grammar's non-terminals. */
%type <program> program
%type <classes> class_list
%type <class_> class
%type <feature> feature
%type <features> feature_list
%type <features> in_features
%type <feature> method
%type <feature> attr
%type <formal> formal
%type <formals> formal_list
%type <case_> case
%type <cases> case_list
%type <expression> expression
%type <expressions> expression_list
%type <expressions> args
%type <expression> inside_let


/* Precedence declarations go here. */
%right LETPREC
%right ASSIGN
%left NOT
%nonassoc LE '<' '='
%left '+' '-'
%left '*' '/'
%left ISVOID
%left '~'
%left '@'
%left '.'

%%
/* 
   Save the root of the abstract syntax tree in a global variable.
*/
program	: class_list	{ ast_root = program($1); }
        ;

class_list
	: class			/* single class */
		{ $$ = single_Classes($1);
                  parse_results = $$; }
	| class_list class	/* several classes */
		{ $$ = append_Classes($1,single_Classes($2)); 
                  parse_results = $$; }
	;

/* If no parent is specified, the class inherits from the Object class. */
class	: CLASS TYPEID '{' feature_list '}' ';'
		{ $$ = class_($2,idtable.add_string("Object"),$4,
			      stringtable.add_string(curr_filename)); }
	| CLASS TYPEID INHERITS TYPEID '{' feature_list '}' ';'
		{ $$ = class_($2,$4,$6,stringtable.add_string(curr_filename)); }
  | CLASS TYPEID INHERITS TYPEID '{' error ';' { $$ = NULL; }
  | CLASS TYPEID '{' error ';' { $$ = NULL; }
  | CLASS TYPEID '{' error '}' ';' { $$ = NULL; }
  | CLASS error '{' feature_list '}' ';' { $$ = NULL; }
  | CLASS error '{' error '}' ';' { $$ = NULL; }
	;

/* Feature list may be empty, but no empty features in list. */
feature_list: in_features { $$ = $1; }
  |   /* empty */
      {  $$ = nil_Features(); }
  ;

in_features: in_features feature {
    $$ = append_Features($1, single_Features($2));
  }
  | feature { $$ = single_Features($1); }
  ;

/* A single feature can be a method or an attribute */
feature: method ';' { $$ = $1; }
  |      attr ';' { $$ = $1; }
  |      error ';' {
    $$ = NULL; 
  }
  ;

method: OBJECTID '(' formal_list ')' ':' TYPEID '{' expression '}' {
    $$ = method($1, $3, $6, $8);
  }
  | OBJECTID '(' ')' ':' TYPEID '{' expression '}' {
    $$ = method($1, nil_Formals(), $5, $7);
  }
  ;

attr: OBJECTID ':' TYPEID { $$ = attr($1, $3, no_expr()); }
  | OBJECTID ':' TYPEID ASSIGN expression { $$ = attr($1, $3, $5); }
  ;

formal_list: formal { $$ = single_Formals($1); }
  | formal_list ',' formal { $$ = append_Formals($1, single_Formals($3)); }
  ;

formal: OBJECTID ':' TYPEID { $$ = formal($1, $3); }
  ;

expression: OBJECTID ASSIGN expression { $$ = assign($1, $3); }
  | expression '.' OBJECTID '(' args ')' {
    $$ = dispatch($1, $3, $5);
  }
  | expression '@' TYPEID '.' OBJECTID '(' args ')' {
    $$ = static_dispatch($1, $3, $5, $7);
  }
  | OBJECTID '(' args ')' {
    $$ = dispatch(object(idtable.add_string("self")), $1, $3);
  }
  | IF expression THEN expression ELSE expression FI {
    $$ = cond($2, $4, $6);
  }
  | WHILE expression LOOP expression POOL {
    $$ = loop($2, $4);
  }
  | '{' expression_list '}' {
    $$ = block($2);
  }
  | LET inside_let { $$ = $2; }
  | CASE expression OF case_list ESAC { $$ = typcase($2, $4); }
  | NEW TYPEID { $$ = new_($2); }
  | ISVOID expression { $$ = isvoid($2); }
  | expression '+' expression { $$ = plus($1, $3); }
  | expression '-' expression { $$ = sub($1, $3); }
  | expression '*' expression { $$ = mul($1, $3); }
  | expression '/' expression { $$ = divide($1, $3); }
  | '~' expression { $$ = neg($2); }
  | expression '<' expression { $$ = lt($1, $3); }
  | expression LE expression { $$ = leq($1, $3); }
  | expression '=' expression { $$ = eq($1, $3); }
  | NOT expression { $$ = comp($2); }
  | '(' expression ')' { $$ = $2; }
  | OBJECTID { $$ = object($1); }
  | INT_CONST { $$ = int_const($1); }
  | STR_CONST { $$ = string_const($1); }
  | BOOL_CONST { $$ = bool_const($1); }
  ;

expression_list: expression ';' { $$ = single_Expressions($1); }
  | expression_list expression ';' { $$ = append_Expressions($1, single_Expressions($2)); }
  | expression_list error ';' { $$ = NULL; }
  | error ';' { $$ = NULL; }
  ;

inside_let: OBJECTID ':' TYPEID IN expression %prec LETPREC { $$ = let($1, $3, no_expr(), $5); }
  | OBJECTID ':' TYPEID ASSIGN expression IN expression %prec LETPREC { $$ = let($1, $3, $5, $7); }
  | OBJECTID ':' TYPEID ',' inside_let { $$ = let($1, $3, no_expr(), $5); }
  | OBJECTID ':' TYPEID ASSIGN expression ',' inside_let { $$ = let($1, $3, $5, $7); }
  | error IN expression %prec LETPREC { $$ = NULL; }
  | error ',' inside_let { $$ = NULL; }
  ;

args: expression { $$ = single_Expressions($1); }
  | args ',' expression { $$ = append_Expressions($1, single_Expressions($3)); }
  | { $$ = nil_Expressions(); }
  ;

case_list: case { $$ = single_Cases($1); }
  | case_list case { $$ = append_Cases($1, single_Cases($2)); }
  ;

case: OBJECTID ':' TYPEID DARROW expression ';' { $$ = branch($1, $3, $5); }
  ;

/* end of grammar */
%%

/* This function is called automatically when Bison detects a parse error. */
void yyerror(char *s)
{
  extern int curr_lineno;

  cerr << "\"" << curr_filename << "\", line " << curr_lineno << ": " \
    << s << " at or near ";
  print_cool_token(yychar);
  cerr << endl;
  omerrs++;

  if(omerrs>50) {fprintf(stdout, "More than 50 errors\n"); exit(1);}
}


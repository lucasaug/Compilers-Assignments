/*
 *  The scanner definition for COOL.
 */

/*
 *  Stuff enclosed in %{ %} in the first section is copied verbatim to the
 *  output, so headers and global definitions are placed here to be visible
 * to the code in the file.  Don't remove anything that was here initially
 */
%{
#include <cool-parse.h>
#include <stringtab.h>
#include <utilities.h>

/* The compiler assumes these identifiers. */
#define yylval cool_yylval
#define yylex  cool_yylex

/* Max size of string constants */
#define MAX_STR_CONST 1025
#define YY_NO_UNPUT   /* keep g++ happy */

extern FILE *fin; /* we read from this file */

/* define YY_INPUT so we read from the FILE fin:
 * This change makes it possible to use this scanner in
 * the Cool compiler.
 */
#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	if ( (result = fread( (char*)buf, sizeof(char), max_size, fin)) < 0) \
		YY_FATAL_ERROR( "read() in flex scanner failed");

char string_buf[MAX_STR_CONST]; /* to assemble string constants */
char *string_buf_ptr;

extern int curr_lineno;
extern int verbose_flag;

extern YYSTYPE cool_yylval;

/* New definitions */

// How many comments have we seen so far that haven't been closed
int comment_depth = 0;

// Macro to insert new character into string
#define insertIntoString(c) { \
	if(string_buf_ptr - string_buf + 1 >= MAX_STR_CONST) { \
		BEGIN(FINISHSTRING);\
		cool_yylval.error_msg = (char *)"String constant too long";\
		return ERROR;\
	} else { \
		*string_buf_ptr = c;\
		string_buf_ptr++;\
	}\
}

%}

/* COOL arithmetic and comparison
 * operators, as well as other unspecified tokens
 */

SINGLECHAR [+\-*/~().@=<{}:,;]

/* Possible states */
%x STRING FINISHSTRING COMMENT

%%

{SINGLECHAR} return yytext[0];


 /* Two-character operations */

"<=" return LE;
"<-" return ASSIGN;
"=>" return DARROW;


 /*
	* Keywords are case-insensitive except for the values true and false,
	* which must begin with a lower-case letter.
	*/

[cC][lL][aA][sS][sS]             return CLASS;
[eE][lL][sS][eE]                 return ELSE;
[fF][iI]                         return FI;
[iI][fF]                         return IF;
[iI][nN]                         return IN;
[iI][nN][hH][eE][rR][iI][tT][sS] return INHERITS;
[iI][sS][vV][oO][iI][dD]         return ISVOID;
[lL][eE][tT]                     return LET;
[lL][oO][oO][pP]                 return LOOP;
[pP][oO][oO][lL]                 return POOL;
[tT][hH][eE][nN]                 return THEN;
[wW][hH][iI][lL][eE]             return WHILE;
[cC][aA][sS][eE]                 return CASE;
[eE][sS][aA][cC]                 return ESAC;
[nN][eE][wW]                     return NEW;
[oO][fF]                         return OF;
[nN][oO][tT]                     return NOT;

f[aA][lL][sS][eE] {
	cool_yylval.boolean = false;
	return BOOL_CONST;
}
t[rR][uU][eE] {
	cool_yylval.boolean = true;
	return BOOL_CONST;
}


 /* Integer, object and type identifiers */

[0-9]+ {
	// Integer value
	cool_yylval.symbol = inttable.add_string(yytext);
	return INT_CONST;
}

[a-z][0-9a-zA-Z_]* {
	// Object identifier
	cool_yylval.symbol = idtable.add_string(yytext);
	return OBJECTID;
}

[A-Z][0-9a-zA-Z_]* {
	// Type identifier
	cool_yylval.symbol = idtable.add_string(yytext);
	return TYPEID;
}


 /*
	*  Comments and nested comments
	*/

"--".*
"(*" {
	comment_depth++;
	BEGIN(COMMENT);
}
<COMMENT>{
	"*)" {
		// End of a comment, descend one level
		if(--comment_depth == 0){
			BEGIN(INITIAL);
		}
	}
	"(*" {
		// Begin inner comment
		comment_depth++;
		BEGIN(COMMENT);
	}
	\n curr_lineno++;
	<<EOF>> {
		BEGIN(INITIAL);
		cool_yylval.error_msg = (char *)"EOF in comment";
		return ERROR;
	}
	.
}


 /*
	*  String constants (C syntax)
	*  Escape sequence \c is accepted for all characters c. Except for 
	*  \n \t \b \f, the result is c.
	*
	*/

"\"" {
	string_buf_ptr = string_buf;
	BEGIN(STRING);
}
<STRING>{
	"\"" {
		BEGIN(INITIAL);
		*string_buf_ptr = '\0';
		cool_yylval.symbol = stringtable.add_string(string_buf);
		return STR_CONST;
	}
	\0 {
		BEGIN(FINISHSTRING);
		cool_yylval.error_msg = (char *)"String contains null character";
		return ERROR;
	}
	\n {
		// We assume the programmer meant to put a \ before the \n, so we add it
		insertIntoString('\n');
		curr_lineno++;
		cool_yylval.error_msg = (char *)"Unterminated string constant";
		return ERROR;
	}
	\\\n {
		curr_lineno++;
		insertIntoString('\n');
	}
	\\n insertIntoString('\n');
	\\t insertIntoString('\t');
	\\b insertIntoString('\b');
	\\f insertIntoString('\f');
	\\. insertIntoString(yytext[1]);
	.   insertIntoString(yytext[0]);
	<<EOF>> {
		BEGIN(INITIAL);
		cool_yylval.error_msg = (char *)"EOF in string constant";
		return ERROR;
	}
}

 /* Seeks the end of an invalid string */
<FINISHSTRING>{
	\n {
		BEGIN(INITIAL);
		curr_lineno++;
	}
	\\\n curr_lineno++;
	"\"" BEGIN(INITIAL);
	.
}

 /* Throws an error if you try to close an unexisting comment */
"*)" {
	cool_yylval.error_msg = (char *)"Unmatched *)";
	return ERROR;
}

 /* Ignore whitespaces and count lines */

[ \f\r\t\v]*
\n	curr_lineno++;

 /* If everything else fails, throws an error and continues */
. {
	cool_yylval.error_msg = yytext;
	return ERROR;
}

%%

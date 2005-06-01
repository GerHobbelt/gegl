/*
	FILE:	common.h
	DESC:	defines structs, enum
*/

#ifndef _COMMON_H_
#define _COMMON_H_

#include "data_type.h"

#ifndef DEBUG
#define DEBUG 0
#endif

typedef enum
{
  DECL,
  NOT_DECL
}TYPE_DECL;

/* DATA TYPES */
typedef enum
{
  TYPE_INT,
  TYPE_FLOAT,
  TYPE_CHANNEL,
  TYPE_CHANNELFLOAT
}DATA_TYPE;

typedef enum
{
  TYPE_SCALER,
  TYPE_VECTOR,
  TYPE_C_VECTOR,
  TYPE_CA_VECTOR,
  TYPE_C_A_VECTOR,
}SV_TYPE;

/* FUNCTIONS */
typedef enum
{
  OP_PLUS,
  OP_MINUS,
  OP_TIMES,
  OP_DIVIDE,
  OP_NEG,
  OP_WP_CLAMP,
  OP_CHANNEL_CLAMP,
  OP_EQUAL,
}FUNCTION;


/* ELEMENT */
typedef struct
{
  DATA_TYPE 	dtype;
  char          string[256];
  SV_TYPE	svtype;
  int		num;
  char		inited;
  char          scope;
}elem_t;

typedef struct
{
  char 	word[20];
  int	token;
}keyword_t;

typedef struct
{
  char	string[20];
}token_t;

typedef struct
{
  char  word[20];
  char	arg;
  int	token;
}dt_keyword_t;

#ifdef  _LEXER_C_
int SCOPE = 0;
char gegl_pixel[256];
#undef  _LEXER_C_
#else
extern int SCOPE;
extern char gegl_pixel[];
#endif


elem_t  	add_sym (char *s, char scope);
elem_t*	 	get_sym (char *sym);
void		init_image_data (char *indent);
int 		get_keyword (char *s);
dt_keyword_t* 	get_dt_keyword (char *s);
int		yylex ();
void    	rm_sym_from_symtab (char scope);

void open_file (char *filename);
void close_file ();


#endif


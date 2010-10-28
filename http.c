/* (c) 2010 Anthony Catel */
/* WIP */
/* gcc -fshort-enums */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define __   -1

#define BYTES_GET(b) \
	*(uint32_t *) b == ((' ' << 24) | ('T' << 16) | ('E' << 8) | 'G')
	
#define BYTES_POST(b) \
	*(uint32_t *) b == (('T' << 24) | ('S' << 16) | ('O' << 8) | 'P')

typedef enum classes {
	C_NUL,	/* BAD CHAR */
	C_SPACE,/* space */
	C_WHITE,/* other whitespace */
	C_CR,	/* \r */
	C_LF,	/* \n */
	C_COLON,/* : */
	C_COMMA,/* , */
	C_QUOTE,/* " */
	C_BACKS,/* \ */
	C_SLASH,/* / */
	C_PLUS, /* + */
	C_MINUS,/* - */
	C_POINT,/* . */
	C_DIGIT,/* 0-9 */
	C_ETC,	/* other */
	C_STAR,	/* * */
	C_E,	/* E */
	C_G,	/* G */
	C_T,	/* T */
	C_P,	/* P */
	C_H,	/* H */
	C_O,	/* O */
	C_S,	/* S */
	NR_CLASSES
} parser_class;


static int ascii_class[128] = {
/*
    This array maps the 128 ASCII characters into character classes.
    The remaining Unicode characters should be mapped to C_ETC.
    Non-whitespace control characters are errors.
*/
	C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,
	C_NUL,      C_NUL,      C_LF,       C_NUL,      C_NUL,      C_CR,       C_NUL,      C_NUL,
	C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,
	C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,      C_NUL,
	
	C_SPACE,  C_ETC,   C_QUOTE,  C_ETC,    C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,    C_ETC,   C_STAR,   C_PLUS,   C_COMMA, C_MINUS, C_POINT, C_SLASH,
	C_DIGIT,  C_DIGIT, C_DIGIT,  C_DIGIT,  C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT,
	C_DIGIT,  C_DIGIT, C_COLON,  C_ETC,    C_ETC,   C_ETC,   C_ETC,   C_ETC,
	
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_E,     C_ETC,   C_G,
	C_H,     C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_O,
	C_P,   	 C_ETC,   C_ETC,   C_S,     C_T,     C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_BACKS, C_ETC,   C_ETC,   C_ETC,
	
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_NUL
};


typedef enum states {
	GO,  /* start    */
	G1, /* GE */
	G2, /* GET */
	G3, /* GET  */
	P1, /* PO */
	P2, /* POS */
	P3, /* POST */
	P4, /* POST  */
	PT, /* path */
	H1, /* H */
	H2, /* HT */
	H3, /* HTT */
	H4, /* HTTP */
	H5, /* HTTP/ */
	H6, /* HTTP/[0-9] */
	H7, /* HTTP/[0-9]. */
	H8, /* HTTP/[0-9].[0-9] */
	EL, /* expect new line */
	ER, /* expect \n */
	HK, /* header key */
	HV, /* header value */
	NR_STATES
} parser_state;

typedef enum type {
	HTTP_PARSE_ERROR,
	HTTP_METHOD,
	HTTP_PATH,
	HTTP_VERSION_MAJOR,
	HTTP_VERSION_MINOR,
	HTTP_HEADER_KEY,
	HTTP_HEADER_VAL,
	HTTP_EOL
} callback_type;

enum methods {
	HTTP_GET,
	HTTP_POST
};

typedef enum actions {
	MG = -2, /* Method get */
	MP = -3, /* Method post */
	PE = -4, /* Path end */
	HA = -5, /* HTTP MAJOR */
	HB = -6, /* HTTP MINOR  */
	KH = -7, /* header key */
	VH = -8 /* header value */
} parser_actions;

typedef int (*HTTP_parser_callback)(void* ctx, callback_type type, int value, uint32_t step);

struct _http_parser {
	HTTP_parser_callback callback; /* user callback function */
	void *ctx; /* user defined */
	uint32_t step; /* char number */
	parser_state state; /* state */
};

static int state_transition_table[NR_STATES][NR_CLASSES] = {
/*  
                       nul   white                                 etc           
                       | space |  \r\n  :  ,  "  \  /  +  -  . 09  |  *  E  G  T  P  H  O  S*/
/*start           GO*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,G1,__,P1,__,__,__},
/*GE              G1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,G2,__,__,__,__,__,__},
/*GET             G2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,G3,__,__,__,__},
/*GET             G3*/ {__,MG,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*PO              P1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,P2,__},
/*POS             P2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,P3},
/*POST            P3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,P4,__,__,__,__},
/*POST            P4*/ {__,MP,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/* path	          PT*/ {__,PE,__,__,__,__,__,__,__,PT,PT,PT,PT,PT,PT,__,PT,PT,PT,PT,PT,__,__},
/*H 	          H1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,H2,__,__},
/*HT	          H2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,H3,__,__,__,__},
/*HTT	          H3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,H4,__,__,__,__},
/*HTTP	          H4*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,H5,__,__,__},
/*HTTP/	          H5*/ {__,__,__,__,__,__,__,__,__,H6,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*HTTP/[0-9]      H6*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,HA,__,__,__,__,__,__,__,__,__},
/*HTTP/[0-9]/     H7*/ {__,__,__,__,__,__,__,__,__,__,__,__,H8,__,__,__,__,__,__,__,__,__,__},
/*HTTP/[0-9]/[0-9]H8*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,HB,__,__,__,__,__,__,__,__,__},
/* new line 	  EL*/ {__,__,__,ER,HK,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/* \r expect \n   ER*/ {__,__,__,__,HK,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/* header key 	  HK*/ {__,__,__,__,__,KH,__,__,__,__,__,HK,__,__,HK,__,HK,HK,HK,HK,HK,__,__},
/* header value   HV*/ {__,HV,__,VH,VH,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,HV,__,__},
};

/* compiled as jump table by gcc */
static int parse_http_char(struct _http_parser *parser, const unsigned char c)
{
	
	parser_class c_classe = ascii_class[c];
	int8_t state;
	
	parser->step++;
	
	if (c_classe == C_NUL) return 0;
	
	
	state = state_transition_table[parser->state][c_classe]; /* state > 0, action < 0 */
	
	if (state >= 0) {
		parser->state = state;
	} else {
		switch(state) {
			case MG: /* GET detected */
				parser->callback(parser->ctx, HTTP_METHOD, HTTP_GET, parser->step);
				parser->state = PT;
				break;
			case MP:
				parser->callback(parser->ctx, HTTP_METHOD, HTTP_POST, parser->step);
				parser->state = PT;
				break;
			case PE: /* End of path */
				parser->callback(parser->ctx, HTTP_PATH, 0, parser->step);
				parser->state = H1;
				break;
			case HA: /* HTTP Major */
				parser->callback(parser->ctx, HTTP_VERSION_MAJOR, c-48, parser->step);
				parser->state = H7;
				break;
			case HB: /* HTTP Minor */
				parser->callback(parser->ctx, HTTP_VERSION_MINOR, c-48, parser->step);
				parser->state = EL;
				break;
			case KH:
				parser->callback(parser->ctx, HTTP_HEADER_KEY, 0, parser->step);
				parser->state = HV;
				break;
			case VH:
				parser->callback(parser->ctx, HTTP_HEADER_VAL, 0, parser->step);
				parser->state = (c_classe == C_CR ? ER : HK); /* \r\n or \n */
				break;				
			default:
				return 0;
		}
	}
	
	return 1;
}

/* also compiled as jump table */
static int parse_callback(void *ctx, callback_type type, int value, uint32_t step)
{
	switch(type) {
		case HTTP_METHOD:
			switch(value) {
				case HTTP_GET:
					printf("GET method detected\n");
					break;
				case HTTP_POST:
					printf("POST method detected\n");
					break;
			}
			break;
		case HTTP_PATH:
			printf("End of path on byte %i\n", step);
			break;
		case HTTP_VERSION_MAJOR:
		case HTTP_VERSION_MINOR:
			printf("Version detected %i\n", value);
			break;
		case HTTP_EOL:
			printf("End of line\n");
			break;
		case HTTP_HEADER_KEY:
			printf("Header key\n");
			break;
		case HTTP_HEADER_VAL:
			printf("Header value\n");
			break;
		default:
			break;
	}
	return 1;
}

/* TEST */
int main()
{
	int length = 0;
	struct _http_parser p;
	
	/* Process BYTE_GET/POST opti check before running the parser */
	
	p.state = GO;
	p.step 	= 0;
	p.ctx 	= &p;
	p.callback = parse_callback;
	
	char chaine[] = "POST /foo/bar/beer HTTP/1.1\nContent-Length: 42\r\nfoo: bar\n";
	
	for (length = 0; length < strlen(chaine); length++) {
		if (parse_http_char(&p, chaine[length]) == 0) {
			printf("fail\n");
			break;
		}
	}
}

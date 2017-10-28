#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

#include "mpc.h"


/* Declare New lval Struct */
typedef struct lval {
    int type;
    long num;
    double dbl;

    /* Error and Symbol types have some string data */
    char* err;
    char* sym;

    /* Count and Pointer to a list of "lval*" */
    int count;
    struct lval** cell;
} lval;

/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_DBL, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

void lval_print( lval* v );
lval* lval_eval( lval* v );

/* Construct a pointer to a new Number lval */
lval* lval_num( long x ) {
    lval* v = malloc( sizeof( lval ) );

    v->type = LVAL_NUM;
    v->num = x;

    return v;
}

/* Create a new double type lval */
lval* lval_dbl( double x ) {
    lval *v= malloc( sizeof( lval ) );

    v->type = LVAL_DBL;
    v->dbl = x;

    return v;
}

/* Construct a pointer to a new Error lval */
lval* lval_err( char* m ) {
    lval* v = malloc( sizeof( lval ) );

    v->type = LVAL_ERR;
    v->err = malloc( strlen( m ) + 1 );
    strcpy( v->err, m );

    return v;
}

/* Construct a pointer to a new Symbol lval */
lval* lval_sym( char* s ) {
    lval* v = malloc( sizeof( lval ) );

    v->type = LVAL_SYM;
    v->sym = malloc( strlen( s ) + 1 );
    strcpy( v->sym, s );

    return v;
}


/* A pointer to a new empty Sexpr lval */
lval* lval_sexpr( void ) {
    lval* v = malloc( sizeof( lval ) );

    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;

    return v;
}

void lval_del(lval* v) {

    switch ( v->type ) {
    /* Do nothing special for number type */
    case LVAL_NUM: break;
    case LVAL_DBL: break;

    /* For Err or Sym free the string data */
    case LVAL_ERR: free( v->err ); break;
    case LVAL_SYM: free( v->sym ); break;

    /* If Sexpr then delete all elements inside */
    case LVAL_SEXPR:
        for ( int i = 0; i < v->count; i++ ) {
            lval_del( v->cell[ i ] );
        }
        /* Also free the memory allocated to contain the pointers */
        free( v->cell );
        break;
    }

    /* Free the memory allocated for the "lval" struct itself */
    free( v );
}

lval* lval_read_num( mpc_ast_t* t ) {
    errno = 0;
    long x = strtol( t->contents, NULL, 10 );

    return errno != ERANGE ?
                lval_num( x ) : lval_err( "invalid number" );
}

lval* lval_read_dbl( mpc_ast_t* t ) {
    errno = 0;
    double x = strtod( t->contents, NULL );

    return errno != ERANGE ?
                lval_dbl( x ) : lval_err( "invalid double" );
}

lval* lval_add( lval* v, lval* x ) {
    v->count++;
    v->cell = realloc( v->cell, sizeof( lval* ) * v->count );
    v->cell[ v->count - 1 ] = x;

    return v;
}

lval* lval_read( mpc_ast_t* t ) {

    /* If Symbol or Number return conversion to that type */
    if ( strstr( t->tag, "number" ) ) { return lval_read_num( t ); }
    if ( strstr( t->tag, "double" ) ) { return lval_read_dbl( t ); }
    if ( strstr( t->tag, "symbol" ) ) { return lval_sym( t->contents ); }

    /* If root (>) or sexpr then create empty list */
    lval* x = NULL;
    if ( strcmp( t->tag, ">" ) == 0 ) { x = lval_sexpr(); }
    if ( strstr( t->tag, "sexpr" ) )  { x = lval_sexpr(); }

    /* Fill this list with any valid expression contained within */
    for ( int i = 0; i < t->children_num; i++ ) {
        if ( strcmp( t->children[ i ]->contents, "(" ) == 0 ) { continue; }
        if ( strcmp( t->children[ i ]->contents, ")" ) == 0 ) { continue; }
        if ( strcmp( t->children[ i ]->tag,  "regex" ) == 0 ) { continue; }
        x = lval_add( x, lval_read( t->children[i] ) );
    }

    return x;
}

void lval_expr_print( lval* v, char open, char close ) {
    putchar( open );
    for ( int i = 0; i < v->count; i++ ) {

        /* Print Value contained within */
        lval_print( v->cell[ i ] );

        /* Don't print trailing space if last element */
        if ( i != ( v->count - 1 ) ) {
            putchar( ' ' );
        }
    }
    putchar( close );
}

void lval_print( lval* v ) {
    switch (v->type) {
    case LVAL_NUM:   printf( "%li", v->num ); break;
    case LVAL_DBL:   printf( "%f", v->dbl ); break;
    case LVAL_ERR:   printf( "Error: %s", v->err ); break;
    case LVAL_SYM:   printf( "%s", v->sym ); break;
    case LVAL_SEXPR: lval_expr_print( v, '(', ')' ); break;
    }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_pop( lval* v, int i ) {
    /* Find the item at "i" */
    lval* x = v->cell[ i ];

    /* Shift memory after the item at "i" over the top */
    memmove( &v->cell[ i ], &v->cell[i + 1], sizeof( lval* ) * ( v->count - i - 1 ) );

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc( v->cell, sizeof( lval* ) * v->count );

    return x;
}

lval* lval_take( lval* v, int i ) {
    lval* x = lval_pop( v, i );
    lval_del( v );

    return x;
}

lval* builtin_op( lval* a, char* op ) {

    /* Ensure all arguments are numbers */
    for ( int i = 0; i < a->count; i++ ) {
        if ( a->cell[ i ]->type != LVAL_NUM) {
            lval_del( a );
            return lval_err( "Cannot operate on non-number!" );
        }
    }

    /* Pop the first element */
    lval* x = lval_pop( a, 0 );

    /* If no arguments and sub then perform unary negation */
    if ( ( strcmp( op, "-" ) == 0 ) && a->count == 0 ) {
        x->num = -x->num;
    }

    /* While there are still elements remaining */
    while ( a->count > 0 ) {

        /* Pop the next element */
        lval* y = lval_pop( a, 0 );

        if ( strcmp( op, "+" ) == 0) {
            if ( x->type == LVAL_DBL && y->type == LVAL_DBL ) {
                return lval_dbl( x->dbl + y->dbl );
            }
            else if ( x->type == LVAL_DBL && y->type == LVAL_NUM ) {
                return lval_dbl( x->dbl + y->num );
            }
            else if ( x->type == LVAL_NUM && y->type == LVAL_DBL ) {
                return lval_dbl( x->num + y->dbl );
            }
            else {
                return lval_num( x->num + y->num );
            }
        }
        if ( strcmp( op, "-" ) == 0) { x->num -= y->num; }
        if ( strcmp( op, "*" ) == 0) { x->num *= y->num; }
        if ( strcmp( op, "/" ) == 0) {
            if ( y->num == 0 ) {
                lval_del( x );
                lval_del( y );
                x = lval_err( "Division By Zero!" ); break;
            }
            x->num /= y->num;
        }

        lval_del( y );
    }

    lval_del( a );

    return x;
}

lval* lval_eval_sexpr( lval* v ) {

    /* Evaluate Children */
    for ( int i = 0; i < v->count; i++ ) {
        v->cell[ i ] = lval_eval( v->cell[ i ] );
    }

    /* Error Checking */
    for ( int i = 0; i < v->count; i++ ) {
        if ( v->cell[ i ]->type == LVAL_ERR ) {
            return lval_take( v, i );
        }
    }

    /* Empty Expression */
    if ( v->count == 0 ) {
        return v;
    }

    /* Single Expression */
    if ( v->count == 1 ) {
        return lval_take( v, 0 );
    }

    /* Ensure First Element is Symbol */
    lval* f = lval_pop( v, 0 );
    if ( f->type != LVAL_SYM ) {
        lval_del( f );
        lval_del(v);
        return lval_err( "S-expression Does not start with symbol!" );
    }

    /* Call builtin with operator */
    lval* result = builtin_op( v, f->sym );
    lval_del( f );

    return result;
}


lval* lval_eval(lval* v) {
    /* Evaluate Sexpressions */
    if ( v->type == LVAL_SEXPR ) {
        return lval_eval_sexpr( v );
    }

    /* All other lval types remain the same */
    return v;
}

int main( int argc, char** argv ) {

    /* Create Some Parsers */
    mpc_parser_t* Number   = mpc_new( "number" );
    mpc_parser_t* Double   = mpc_new( "double" );
    mpc_parser_t* Symbol   = mpc_new( "symbol" );
    mpc_parser_t* Sexpr    = mpc_new( "sexpr" );
    mpc_parser_t* Expr     = mpc_new( "expr" );
    mpc_parser_t* Lispy    = mpc_new( "lispy" );

    /* Define them with the following Language */
    mpca_lang( MPCA_LANG_DEFAULT,
               "                                                              \
               number   : /-?[0-9]+/ ;                                        \
               double   : /-?[0-9]+\\.[0-9]+/;                                \
               symbol   : '+' | '-' | '*' | '/' | '^' | '%' ;                 \
               sexpr    : '(' <expr>* ')' ;                                   \
               expr     : <double> | <number> | <symbol> | <sexpr> ;          \
               lispy    : /^/ <expr>+ /$/ ;                        \
               ",
               Number, Double, Symbol, Sexpr, Expr, Lispy );

    /* Print Version and Exit Information */
    puts( "Lispy Version 0.0.0.0.5" );
    puts( "Press Ctrl+c to Exit\n" );

    /* In a never ending loop */
    while ( 1 ) {

        /* Output our prompt and get input */
        char* input = readline( "lispy> " );

        /* Add input to history */
        add_history( input );

        /* Attempt to Parse the user Input */
        mpc_result_t r;
        if ( mpc_parse( "<stdin>", input, Lispy, &r ) ) {
            /* On Success Eval the AST */

            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);

            mpc_ast_delete( r.output );
        } else {
            /* Otherwise Print the Error */
            mpc_err_print( r.error );
            mpc_err_delete( r.error );
        }

        /* Free retrieved input */
        free( input );

    }

    /* Undefine and Delete our Parsers */
    mpc_cleanup( 6, Number, Double, Symbol, Sexpr, Expr, Lispy);

    return 0;
}

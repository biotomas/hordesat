/*
 Incrementally solve DIMACS formulas
 
 A single command-line argument specifying the file that
 contains the update instructions is expected
 */


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <limits.h>
#include "../ipasir.h"

#define QDPLL_RESULT_UNKNOWN 0
#define QDPLL_RESULT_SAT 10
#define QDPLL_RESULT_UNSAT 20

/* symbol to separate update instructions */
#define SEP 'X'

typedef void QDPLL;
typedef int QDPLLResult;
typedef int LitID;

struct list {
    int frame;
    struct list *next;
};
typedef struct list pop_list;

/* symbols use to represent push, resp. pop, operations */
#define PUSH '<'
#define POP '>'
#define ADD '+'   /* this is a permanent push operation */

/* maximal length of any filename */
#define MAX_FILE_NAME 512

/* macros for the parser */
#define PARSER_READ_NUM(num, c)                        \
assert (isdigit (c));                                \
num = 0;					       \
do						       \
{						       \
num = num * 10 + (c - '0');		       \
}						       \
while (isdigit ((c = getc (in))));

#define PARSER_SKIP_SPACE_DO_WHILE(c)		     \
do						     \
{                                                \
c = getc (in);				     \
}                                                \
while (isspace (c));

#define PARSER_SKIP_SPACE_WHILE(c)		     \
while (isspace (c))                                \
c = getc (in);
/* **************************************************** */

/*
 incrementally update solver instance 'qdpll' using the update instructions provided in file 'inp'
 returns EOF if end of input stream is reached
 */
int update_ipasir(QDPLL *qdpll, FILE *inp, pop_list **pl);


/* main function */
int main(int argc, char *argv[]) {
	ipasir_setup(argc, argv);
	
	FILE *io;			/* the input file that contain the update instructions needed for incremental solving */
	QDPLLResult res;	/* result returned by the solver */
	time_t t_start, t_end, t_duration;
	time_t total_time = 0;
    pop_list *pl = NULL;    /* popped frame ids */
		
    /* check command line argument */
    if (argc < 2) {
        fprintf(stderr, "Input file is expected as only argument!\n");
        exit(EXIT_FAILURE);
    }
    
    /* open input formula */
    io = fopen(argv[1], "r");
    if (!io) {
        fprintf(stderr, "ERROR: could not open file %s.\n", argv[1]);
        exit(EXIT_FAILURE);
    }


	/* Create solver instance. */
    QDPLL *depqbf = NULL; //ipasir_init ();
    
	/* update and run solver */
	int i = 1;
    int ret;
    do {
		
        ret = update_ipasir(depqbf, io, &pl); /* incrementally update prefix and matrix */
        if (ret == EOF) {
            break;
        }
        
		printf("\nPart %i of %s:\n", i++, argv[1]);
        fflush(stdout);
    
		t_start = time(0);
        res = ipasir_solve(depqbf);
		t_end = time(0);
        
		switch (res) {				/* output result */
			case QDPLL_RESULT_UNKNOWN:
				printf("UNKNOWN\n");
				break;
			case QDPLL_RESULT_SAT:
				printf("SAT\n");
				break;
			case QDPLL_RESULT_UNSAT:
				printf("UNSAT\n");
				break;
			default:
				assert(0); 
				break;
		}	
		t_duration = t_end - t_start;
		total_time += t_duration;
		printf("real %ld\n", t_duration); /* output runtime */
    } while (ret != EOF);
    
	printf("\n Total time for solving: %ld\n", total_time); /* output total time for solving */
	
	/* clean up */
    ipasir_release (depqbf);
    fclose(io);
    pop_list *tmp;
    while (pl) {
        tmp = pl->next;
        free(pl);
        pl = tmp;
    }
    return 0;
}


/*
 incrementally update solver instance 'qdpll' using the update instructions provided in file 'inp'
 returns EOF if end of input stream is reached
 */
int update_ipasir(QDPLL *qdpll, FILE *input, pop_list **pl) {
    
    FILE *in = input;	/* input stream with update instructions */
    static int frame_index = 0; /* index of the next unused frame ID */
    pop_list *elem;
    int push = 0;
    
    LitID num = 0;
    int neg = 0;
    
    int maxid = 0; /* maximal variable id */
    
    assert (in);
    
    int c;
    while ((c = getc (in)) != EOF && c != SEP)
    {
        PARSER_SKIP_SPACE_WHILE (c);
        
        /* scan problem line */
        if (c == 'p')
        {
            /* parse preamble */
            PARSER_SKIP_SPACE_DO_WHILE (c);
            if (c != 'c')
            goto MALFORMED_PREAMBLE;
            PARSER_SKIP_SPACE_DO_WHILE (c);
            if (c != 'n')
            goto MALFORMED_PREAMBLE;
            PARSER_SKIP_SPACE_DO_WHILE (c);
            if (c != 'f')
            goto MALFORMED_PREAMBLE;
            PARSER_SKIP_SPACE_DO_WHILE (c);
            if (!isdigit (c))
            goto MALFORMED_PREAMBLE;
            
            /* read number of variables */
            PARSER_READ_NUM (maxid, c);
            
            ungetc(c,in);
            continue; /* exit while loop */
            
        MALFORMED_PREAMBLE:
            fprintf(stderr, "ERROR: malformed problem line in input file!\n");
            exit(EXIT_FAILURE);
        }
        
    
        PARSER_SKIP_SPACE_WHILE (c);
        
        if (c == ADD) {
            /* do nothing, clauses are added permanently */
            push = 0;
            continue;
        }
        
        if (c == PUSH) {
            /* push frame on the stack */
            push=1;
            frame_index++;
            continue;
        }
        
        
        if (c == POP) {
            /* pop frame from the stack */
            elem = (pop_list*)malloc(sizeof(pop_list));
            elem->frame = frame_index;
            elem->next = *pl;
            *pl = elem;
            
            continue;
        }
        
        
        if (!isdigit (c) && c != '-') {
            if (c == EOF || c == SEP)
                return c;
            fprintf(stderr, "Expecting digit or \'-\' in input file!\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            if (c == '-')
            {
                neg = 1;
                if (!isdigit ((c = getc (in))))
                {
                    fprintf(stderr, "Expecting digit in input file!\n");
                    exit(EXIT_FAILURE);
                }
            }
            
            /* parse a literal  */
            PARSER_READ_NUM (num, c);
            num = neg ? -num : num;
            
            
            if (num == 0 && push==1) {
                ipasir_add(qdpll, maxid+frame_index); /* add frame literal */
            }
            ipasir_add(qdpll,num);
            if (num >= maxid+frame_index)
            {
                fprintf(stderr, "Adding literal %d: identifier too large!\n", num);
                exit(EXIT_FAILURE);
            }

            neg = 0;
        }
    }
    
    /* activate all frames */
    int i = 1;
    int ptest = 0;
    for (; i <= frame_index; i++) {
        /* check if i is contained in the popped list */
        ptest = 0;
        elem = *pl;
        while (elem) {
            if (elem->frame == i) {
                ptest = 1;
                break;
            }
            elem = elem->next;
        }
        
        if (!ptest)
            ipasir_assume(qdpll, -(i+maxid));
    }
    /* deactivate the popped ones */
    elem = *pl;
    while (elem) {
        ipasir_assume(qdpll, (elem->frame)+maxid);
        elem = elem->next;
    }
    
    return c;
}




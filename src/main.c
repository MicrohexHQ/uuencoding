//main.c

//created by: Kurt L. Manion
//on: 16 Aug 2016
//
//problem from: «https://www.reddit.com/r/dailyprogrammer/comments/4xy6i1/20160816_challenge_279_easy_uuencoding/»
//
//uuencoding/uudecoding

//example encoded file that reads "cat"
/*
  begin 644 cat.txt
  #0V%T
  `
  end
*/

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <string.h>
#include <err.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>

//#define _use_kprintf_
#ifdef _use_kprintf_
#define kprintf(...) do{ (void)fprintf(stderr, __VA_ARGS__); }while(0)
#else
#define kprintf(...) /* NULL */
#endif /* _use_kprintf_ */

#define EBADIN 79
#define EBADOUT 80

#define LINE_BUF ((size_t)4096)

const char * const opts = "edo:f:ch";
const struct option longopts[] = {
	{ "encode",	no_argument,		NULL,	'e' },
	{ "decode",	no_argument,		NULL,	'd' },
	{ "output",	required_argument,	NULL,	'o' },
	{ "file",	required_argument,	NULL,	'f' },
	{ "cat",	no_argument,		NULL,	'c' },
	{ "help",	no_argument,		NULL,	'h' },
	{ NULL, 0, NULL, 0 }
};

void __dead2
usage(void)
	{
		(void)fprintf(stderr, "usage:\n%s\n%s\n", 
			"`basename` -e -f file_to_encode -o output_file",
			"`basename` -d -f file_to_decode");
		exit(EX_USAGE);
	}

uint8_t encode(const char * restrict ifile, const char * restrict ofile);
uint8_t decode(const char * restrict ifile);
int cat(const char *file);

int
main(
	int argc,
	char * const argv[])
	{
		extern int optind;
		extern char *optarg;
		char flg;
		uint8_t e_flg, d_flg, c_flg;
		char *output_file = NULL;
		char *input_file = NULL;

		e_flg = d_flg = c_flg = 0;

		while ((flg = getopt_long(argc,argv, opts, longopts, NULL)) != -1) {
			switch (flg) {
			case 'e':
				e_flg = 1;
				break;;

			case 'd':
				d_flg = 1;
				break;;

			case 'o':
				output_file = optarg;
				break;;
			
			case 'f':
				input_file = optarg;
				break;;

			case 'c':
				c_flg = 1;
				break;;

			case 'h':
			case '?':
				usage();
			}
		}
		if ((e_flg == 1 && d_flg == 1) || (e_flg == 0 && d_flg == 0)) {
			warnx("%s", "you must specify the e or d option");
			usage();
		}
		uint8_t r;
		if (e_flg) //-e -f file -o file
		{
			if (output_file == NULL && c_flg != 1) {
				warnx("%s\n", "no output file has been specified");
				usage();
			}
			if (input_file == NULL) {
				warnx("%s", "no input file has been specified");
				usage();
			}
			r = encode(input_file, output_file);
			if (c_flg) {
				cat(output_file);
			}
			return r;
		}		
		else if (d_flg) //-d -f file
		{
			if (input_file == NULL) {
				warnx("%s", "no input file has been specified");
				usage();
			}
			r = decode(input_file);
			return r;
		}
		return(EXIT_FAILURE);
	}


#define __default_perm__ NULL
#define DEFAULT_PERM __default_perm__
void
header(
	FILE * fd, 				//output file written to in encode funct
	const char *perm, 		//octal UNIX permissions, usualy 644
	const char *filename) 	//filename to be written to
	{
		if (perm == NULL)
			perm = "644";
		fprintf(fd, "begin %s %s\n", perm, filename);
		return;
	}

void
footer(
	FILE *fd)
	{
		(void)fprintf(fd, "`\nend\n");
	}

/* calculates how many bits the array resulting from stobin will have
 * makeing sure that it has padding bits to make it a multiple of 6 */
size_t
stobin_size(
	const char *s)
	{
		size_t l;
		l = (strlen(s))*8;
		//add padding bits
		if (l%6 != 0)
			l += 6 - (l%6);
		return l;
	}

/* converts an array of chars to an array of 1s and 0s
 * skips the ending newline
 * returned value must be freed */
uint8_t*
stobin(
	const char *s)
	{
		uint8_t *a;
		size_t a_len;
		int mask[] = { 0x1, 0x2, 0x4, 0x8,
						0x10, 0x20, 0x40, 0x80 };
		a_len = stobin_size(s);
		a = (uint8_t *)malloc(a_len * sizeof(uint8_t));
		memset(a, 0, (a_len));
		for(size_t i=0,j=0,len=strlen(s); i<len; ++i) {
			//for each character to be encoded
			for(int k=7; k>=0; --k,++j) {
				//for each bit in that character
				a[j] = (s[i] & mask[k]) >> k;
			}
		}
		return a;
	}

/* takes first 6 elements from uint8_t array and considering them as bits
 * converts them to their integer equivalent */
 uint8_t
 bitstoc(
 	uint8_t *a)
	{
		uint8_t acc=0;
		for(int i=0; i<6; ++i)
		{
			acc += a[i] << ((6-1)-i);
		}
		return acc;
	}

/* power-house function that actualy converts text to binary 
 * this is now the thread function
 * --> dose sprintf require the void ?
 * return value needs to be freed
 */
void*
encode_line(
	void *arg)
	{
		char *line = (char *)arg;
		kprintf("encoding line: %s\n", line);
		uint8_t *bin;
		//the 45->60 chars and the length newline and null
		char *enc = (char *)malloc(63*sizeof(char));
		
		//first encode the number of characters
		(void)sprintf(enc, "%c", (int)(strlen(line)+32));

		//convert the string of chars into an array of bits
		bin = stobin(line);//must be freed
		for(size_t i=0,len=stobin_size(line); i<len; i+=6)//check the <=
		{
			(void)sprintf(enc, "%s%c", enc, bitstoc(&bin[i]) + 32);
		}
		(void)sprintf(enc, "%s\n", enc);
		free(bin);
		free(line);//experimental
		return (void *)enc;
	}


/*
 * reads at least 45 characters from the file
 * seeking forward
 * !!! this dose not capture the newline
 * returns EOF on EOF
 * returns 0 otherwise
 */
int
readline(
	char **line,
	FILE *fd)
	{
		char *x;
		*line = (char *)malloc(47*sizeof(char));
		(void)fgets(*line, 46, fd);
		//strip newline
		x = strchr(*line, '\n');
		if (x != NULL)
			*x = '\0';
		if (feof(fd)) {
			return EOF;
		} else {
			return 0;
		}
	}

/*
 * linked-list structure to dynamically hold encoded lines
 */
typedef
struct _enclist_t {
	pthread_t thread;
	struct _enclist_t *cdr;
} Enclist;
#define Enclist_sz (sizeof(Enclist))

/* debugging purposes only */
int
enclist_length(
	Enclist *el)
	{
		if (el && el->cdr) {
			return 1 + enclist_length(el->cdr);
		} else if (el) {
			return 1;
		} else {
			return 0;
		}
	}
		

pthread_t*
newthread(
	Enclist *el)
	{
		if (el && el->cdr != NULL) {
			return newthread(el->cdr);
		} else if (el) {
			(*el).cdr = (Enclist *)malloc(Enclist_sz);
			el->cdr->cdr = NULL;
			return &el->cdr->thread;
		} else {
			warnx("%s\n", "newthread() called with null Enclist");
			return NULL;
		}
	}


void
writeenc(
	Enclist *el,
	FILE * fd)
	{
		void *enc = NULL;
		pthread_join(el->thread, &enc);
		if (enc)
			(void)fprintf(fd, "%s", (char *)enc);
		free(enc);
		if (el->cdr)
			writeenc(el->cdr, fd);
	}

void*
freeenc(
	Enclist *el)
	{
		if (el->cdr)
			el->cdr = freeenc(el->cdr);
		free(el);
		return el = NULL;
	}


/* start a thread for each line
 * storing the thread ids in order
 * in a linked list
 * then join them, writting their return value to f_write
 * change the encode_line funct to only return an encoded version
 * of line (having no aftereffects, since it no longer writes to the file
 * itself
 */
uint8_t
encode(
	const char * restrict ifile,
	const char * restrict ofile)
	{
		FILE * f_read;
		FILE * f_write;
		char *line = NULL;

		if (ifile == NULL)
			errx(EBADIN, "%s\n", "invalid name for input_file");
		if (ofile == NULL)
			errx(EBADOUT, "%s\n", "invalid name for output_file");
		
		f_read = fopen(ifile, "r");
		f_write = fopen(ofile, "w");

		if (!f_read)
			errx(EBADIN, "%s\n", "could not open input_file");
		if (!f_write)
			errx(EBADOUT, "%s\n", "could not open output_file");

		header(f_write, DEFAULT_PERM, ifile);

		Enclist *el = (Enclist *)malloc(Enclist_sz);
		el->cdr = NULL;
		while (readline(&line, f_read) != EOF)
		{
			pthread_create(newthread(el), NULL, &encode_line, (void *)line);

			line = NULL;
		}
		writeenc(el, f_write);
		el = freeenc(el);

		//add footer
		footer(f_write);

		fclose(f_read);
		fclose(f_write);
		return 0; //stand-in
	}


uint8_t
decode(
	const char * restrict ifile)
	{
		kprintf("%s\n", "decode dose nothing as of yet");
		return 0; //stand-in
	}


int cat(
	const char *file)
	{
		char cmd[80];
		snprintf(cmd, 79, "cat %s", file);
		return system(cmd);
	}

/* vim: set ts=4 sw=4 noexpandtab: */

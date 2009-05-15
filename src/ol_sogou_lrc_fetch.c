#include "ol_lrc_fetch.h"
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<errno.h>

#define PREFIX_PAGE_SOGOU "http://mp3.sogou.com/gecisearch.so?query="
#define PREFIX_LRC_SOGOU "http://mp3.sogou.com/"
#define LRC_CLUE_SOGOU "downlrc"
#define TRY_MATCH_MAX 5

static int
convert_to_gbk(const char *init, char *target, size_t t_size)
{		
	int len;
    char *pt;
	
	len = strlen(init);
	if((pt = malloc(len + 1)) == NULL)
		return -1;
	strcpy(pt, init);
	pt[len] = 0;
	convert(charset==NULL?"UTF-8":charset, "GBK", pt, len, target, t_size);
	free(pt);
	return 0;
}

struct lrc_tsu *
sogou_lrc_search(const char *title, const char *singer, int *size)
{
	static struct lrc_tsu result[TRY_MATCH_MAX];
	char page_url[URL_LEN_MAX];
	char title_buf[BUFSIZE];
	char singer_buf[BUFSIZE];
	char buf[BUFSIZE], buf2[BUFSIZE];
	char tmpfilenam[] = "tmplrc-XXXXXX";
	char *ptitle, *psinger;
	char *ptr, *tp, *p;
	FILE *fp;
	int fd, ret, bl1, bl2, count=0;

	memset(result, 0, sizeof(result));
	if(title == NULL && singer == NULL)
		return NULL;

    if(title != NULL) {
		convert_to_gbk(title, buf, BUFSIZE);
		url_encoding(buf, strlen(buf), title_buf, BUFSIZE);
	}
	if(singer != NULL) {
		convert_to_gbk(singer, buf, BUFSIZE);
		url_encoding(buf, strlen(buf), singer_buf, BUFSIZE);
	}

	strcpy(page_url, PREFIX_PAGE_SOGOU);
	if(title != NULL) {
		strcat(page_url, title_buf);
		if(singer != NULL) {
			strcat(page_url, "-");
			strcat(page_url, singer_buf);
		}
	} else 
		strcat(page_url, singer_buf);

	if((fd = mkstemp(tmpfilenam)) < 0)
		return NULL;
    if((fp = fdopen(fd, "w+")) == NULL)
		return NULL;

	if((ret = fetch_into_file(page_url, fp)) < 0)
		return NULL;
	rewind(fp);
    while(fgets(buf, BUFSIZE, fp) != NULL && count<TRY_MATCH_MAX) {
		if((ptr = strstr(buf, LRC_CLUE_SOGOU)) != NULL) {
			tp = strchr(ptr, '"');
			if(tp != NULL)
				*tp = 0;
			tp = strrchr(ptr, '=');
			tp++;
			/* 
			 * no matter Chinese or English, 
			 * the title and singer are separated by "-"
			 */
			p = strchr(tp, '-'); 
			/* singer */
			++p;
			bl2 = singer==NULL? 1 : ignore_case_strcmp(singer_buf, p, strlen(singer_buf))==0;
			url_decoding(p, strlen(p), buf2, BUFSIZE);
			convert("GBK", charset==NULL?"UTF-8":charset, buf2, strlen(buf2), result[count].singer, TS_LEN_MAX);
			*(--p) = 0;
			/* title */
			bl1 = title==NULL ? 1 : ignore_case_strcmp(title_buf, tp, strlen(title_buf))==0;
			url_decoding(tp, strlen(tp), buf2, BUFSIZE);
			convert("GBK", charset==NULL?"UTF-8":charset, buf2, strlen(buf2), result[count].title, TS_LEN_MAX);
			/* restore the url */
			*p = '-';

			if(bl1 && bl2) {
				strcpy(result[count].url, PREFIX_LRC_SOGOU);
				strcat(result[count].url, ptr);
				count++;
			}
		}
	}
	*size = count;
	fclose(fp);
	remove(tmpfilenam);
	return result;
}

int 
sogou_lrc_download(struct lrc_tsu *tsu, const char *pathname)
{
	char *lrc_conv, *pathbuf;
	FILE *fp;
	int ret;
	struct memo lrc;
	lrc.mem_base = NULL;
	lrc.mem_len = 0;

	if((ret = fetch_into_memory(tsu->url, &lrc)) < 0)
		return -1;
	lrc_conv = calloc(lrc.mem_len*2, sizeof(char));
	convert("GBK", charset==NULL ? "UTF-8" : charset, lrc.mem_base, lrc.mem_len, lrc_conv, lrc.mem_len*2);
	free(lrc.mem_base);
	
	pathbuf = path_alloc();
	if(pathname == NULL)
		strcpy(pathbuf, "./");
	else
		strcpy(pathbuf, pathname);

	if(pathbuf[strlen(pathbuf)-1] != '/') {
		pathbuf[strlen(pathbuf)] = '/';
		pathbuf[strlen(pathbuf)+1] = 0;
	}
	strcat(pathbuf, tsu->title);
	strcat(pathbuf, "-");
	strcat(pathbuf, tsu->singer);
	strcat(pathbuf, ".lrc");
	
	if((fp = fopen(pathbuf, "w")) == NULL)
		return -1;
	fputs(lrc_conv, fp);
	fclose(fp);
	free(pathbuf);
	free(lrc_conv);

	return 0;
}

struct lrc_interface sogou = {
	sogou_lrc_search,
	sogou_lrc_download,
};
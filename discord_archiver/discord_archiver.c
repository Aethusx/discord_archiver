#pragma comment(lib, "../curl/libcurldll.a")

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../sqlite/sqlite3.h"
#include "../cJSON/cJSON.h"
#include "../curl/curl.h"

/* struct string */
struct string {
	char *ptr;
	size_t len;
};

/* global variables */
struct string last_id;
short verbose = 0;

/* function for parsing json output from curl */
int parse_message(cJSON *array, sqlite3 *db)
{
	/* init json and sqlite variables */
	cJSON *item = array ? array->child : 0;
	int rc;
	//char *err_msg = 0; //used in executing sqlite not using bind
	
	while (item)
	{
		/* get neccesary json objects */
		cJSON *content = cJSON_GetObjectItem(item, "content");
		cJSON *id = cJSON_GetObjectItem(item, "id");
		cJSON *timestamp = cJSON_GetObjectItem(item, "timestamp");

		/* attachments and attachments content */
		cJSON *attachments = cJSON_GetArrayItem(cJSON_GetObjectItem(item, "attachments"), 0);
		 cJSON *attachments_url = cJSON_CreateString("NULL"); /* default val if there is no attachment */
		 cJSON *attachments_name = cJSON_CreateString("NULL"); /* default val if there is no attachment */
		
		/* author and author content */
		cJSON *author = cJSON_GetObjectItem(item, "author");
		 cJSON *username = cJSON_GetObjectItem(author, "username");
		 cJSON *discriminator = cJSON_GetObjectItem(author, "discriminator");

		/* update last_id */
		last_id.ptr = id->valuestring;

		/* if there is attachment set attachments->url and attachments->file_name*/
		if (cJSON_GetArraySize(attachments) != 0)
		{
			attachments_url = cJSON_GetObjectItem(attachments, "url");
			attachments_name = cJSON_GetObjectItem(attachments, "filename");
		}
			
		/* prepare sqlite statement */
		sqlite3_stmt *stmt;
		sqlite3_prepare_v2(db, "INSERT INTO Messages VALUES(?, ?, ?, ?, ?, ?, ?);", -1, &stmt, NULL);
		//printf("%s#%s: %s\n", username->valuestring, discriminator->valuestring, content->valuestring); //debug

		/* bind values to sqlite statement */
		sqlite3_bind_int64(stmt, 1, strtoll(id->valuestring, NULL, 10)); /* strtoll is workaround, valueint && valuedouble dont work correctly */
		sqlite3_bind_text(stmt, 2, timestamp->valuestring, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 3, username->valuestring, -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt, 4, atoi(discriminator->valuestring)); /* atoi is workaround, valueint && valuedouble dont work correctly  */
		sqlite3_bind_text(stmt, 5, content->valuestring, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 6, attachments_url->valuestring, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 7, attachments_name->valuestring, -1, SQLITE_STATIC);
		
		/* execute statement */
		rc = sqlite3_step(stmt);

		/* check for errors */
		if (rc != SQLITE_DONE)
			printf("ERROR inserting data: %s\n", sqlite3_errmsg(db));

		/* finalize our statement */
		sqlite3_finalize(stmt);
		 
		/*//NOT SAFE !! Use bind not raw execute
		char *sql[512];
		sqlite3_mprintf(sql, "INSERT INTO Messages VALUES(%s, '%s', '%s', %s, '%s', '%s');", id->valuestring, timestamp->valuestring ,username->valuestring, discriminator->valuestring, content->valuestring, attachments_url->valuestring);
		rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

		// check for errors
		if (rc != SQLITE_OK)
			fprintf(stderr, "SQL error: %s\n", err_msg);*/

		/* if verbose write output*/
		if (verbose == 1)
			printf("%s#%s: %s\n", username->valuestring, discriminator->valuestring, content->valuestring);

		/* set next object */
		item = item->next;
	}

	/* if array size is 0 that means last_id == last message on channel so we can pass 1 to indicate that job is done */
	if (cJSON_GetArraySize(array) == 0)
		return 1;

	//printf("\nEnd, last ID: %s \nArray Size:%i\n", last_id.ptr, cJSON_GetArraySize(array)); //for debug
	/* return 0 if array size is not 0 */
	return 0;
}

/* function that init struct string used in discord_get_messagelist*/
void init_string(struct string *s) 
{
	s->len = 0;
	s->ptr = malloc(s->len + 1);
	
	/* if initialized string is null ? ??*/
	#ifdef DEBUG
	if (s->ptr == NULL) 
	{
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	#endif
	
	s->ptr[0] = '\0';
}

/* write function for curl in discord_get_messagelist */
size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
	size_t new_len = s->len + size * nmemb;
	s->ptr = realloc(s->ptr, new_len + 1);
	
	/* if string is null ? ??*/
	#ifdef DEBUG
	if (s->ptr == NULL) 
	{
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	#endif
	
	memcpy(s->ptr + s->len, ptr, size*nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size * nmemb;
}

/* get all discord messages and parse json */
int discord_get_messagelist(char *channel_id, char *token, sqlite3 *db)
{
	/* init curl */
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();

	/* check if curl exists */
	if (!curl)
		return 5;

	/* init string response */
	struct string response;
	init_string(&response);

	/* if there is no last id */
	if (strcmp(last_id.ptr, "001") == 0)
	{
		/* get token and set into curl option */
		struct curl_slist *chunk = NULL;
		char token_auth[128];
		sprintf(token_auth, "Authorization: %s", token);
		chunk = curl_slist_append(chunk, token_auth);
		res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

		/* get first message list*/
		char url[256];
		sprintf(url, "https://discordapp.com/api/v6/channels/%s/messages?limit=100", channel_id);

		/* set curl options */
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE); /* ssl fix */
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		
		/* perform curl request */
		res = curl_easy_perform(curl);
		
		/* check if curl request was success*/
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		/* parse first message list */
		parse_message(cJSON_Parse(response.ptr), db);

		/* cleanup after work */
		free(response.ptr);
		curl_easy_reset(curl);
		return 0;
	}

	/* get token and set into cURL header */
	struct curl_slist *chunk = NULL;
	char token_auth[128];
	sprintf(token_auth, "Authorization: %s", token);
	chunk = curl_slist_append(chunk, token_auth);
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	/* get first message list*/
	char url[256];
	sprintf(url, "https://discordapp.com/api/v6/channels/%s/messages?before=%s&limit=100", channel_id, last_id.ptr);

	/* set curl options */
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE); /* ssl fix */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
	
	/* perform request and check for errors */
	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

	/* parse first message list */
	int parse_response = 0;
	if (parse_message(cJSON_Parse(response.ptr), db) == 1)
		return 1;

	/* cleanup after work */
	free(response.ptr);
	curl_easy_reset(curl);
	return 0;
}

/* function for download_images curl */
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

/* function for downloading all images from sqlite database */
void download_images(sqlite3 *db)
{
	/* init sqlite statement, Attachment_URL is needed for url to download, Attachment_Filename is needed for writing file to this name, Id is needed for creating unique filename */
	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare(db, "select Attachment_URL,Attachment_Filename,Id from Messages where NOT (Attachment_URL = 'NULL')", -1, &stmt, 0);

	/* check for sqlite errors */
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
		return;
	}
	
	/* evaluate our statement */
	rc = sqlite3_step(stmt);

	/* init curl and file stream */
	CURL *curl;
	FILE *fp;
	CURLcode res;
	curl = curl_easy_init();
	
	/* iterate sqlite statement */
	while (rc == SQLITE_ROW)
	{
		/* create better curl error handling */
		if (!curl)
			break;
		
		/* set variables, note: outfilename needs unique identificator because files with same name can overwrite! */
		char *url = sqlite3_column_text(stmt, 0);
		char outfilename[512];
		
		/* change output directoryif you are using unix */
		sprintf(outfilename, "C:\\download\\%s_%s", sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 1));
		
		/* open file stream */
		fp = fopen(outfilename, "wb");
		
		/* set curl options */
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		
		/* perform curl request */
		res = curl_easy_perform(curl);
		
		/* reset curl for next request and close file stream */
		curl_easy_reset(curl);
		fclose(fp);
		
		/* if verbose write output*/
		if(verbose == 1)
			printf("\n\nURL: '%s' \nFilename: '%s'", url, outfilename);

		/* evaluate our statement */
		rc = sqlite3_step(stmt);
	}
	/* end our statement */
	sqlite3_finalize(stmt);
}

/* main function */
int main(int argc, char *argv[])
{
	/* 0 arg file name, 1 arg channel id, 2 arg token, 3 database name, 4 (optional) verbose */
	if (argc < 4)
		return 6;

	/* check if channel id length is smaller than 32 */
	if (strlen(argv[1]) >= 32)
	{
		fprintf(stderr, "Incorrect channel id argument - id is too long.\n");
		return 7;
	}
	
	/* check if channel id length is smaller than 64 */
	if (strlen(argv[2]) >= 64)
	{
		fprintf(stderr, "Incorrect token argument - token is too long.\n");
		return 8;
	}
	
	/* check if channel id length is smaller than 159 */
	if (strlen(argv[3]) >= 159)
	{
		fprintf(stderr, "Incorrect database name argument - file name is too long.\n");
		return 9;
	}
	
	/* check argument only if argument isn't null */
	if (argv[4] != NULL)
		if (strcmp(argv[4], "1") == 0)
			verbose = 1;

	/* init last_id and set to 001 */
	init_string(&last_id);
	last_id.ptr = "001";

	/* init database */
	sqlite3 *db;
	char *err_msg = 0;
	int rc = sqlite3_open(argv[3], &db);

	/* check if database has been opened correctly */
	if (rc != SQLITE_OK) 
	{
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	
	/* drop and create new table */
	char *sql = "DROP TABLE IF EXISTS Messages;"
		"CREATE TABLE Messages(Id Bigint, Timestamp Text, Name Text, Tag Smallint, Message Text, Attachment_URL Text, Attachment_Filename Text);";
	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	
	/* check if database has been created correctly */
	if (rc != SQLITE_OK) 
	{
		fprintf(stderr, "SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		return 1;
	}

	/* init curl */
	curl_global_init(CURL_GLOBAL_ALL);

	/* get messages until end of channel using given program arguments and pass opened sqlite database */
	while(!discord_get_messagelist(argv[1], argv[2], db));
	
	/* download all images from database */
	download_images(db);
	
	/* cleanup after work and return 0 */
	curl_global_cleanup();
	sqlite3_close(db);
	return 0;
}
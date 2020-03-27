#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h> //access
#include "../sqlite/sqlite3.h"
#include "../cJSON/cJSON.h"
#include "../curl/curl.h"

/* struct string */
struct string {
	char *ptr;
	size_t len;
};

/* linked list for cache */
typedef struct node {
	long long int val; /* id of user */
	int val2; /* id in database */
	struct node * next;
} node_t;

/* global variables */
struct string last_id;
short verbose = 0;
node_t * cache = NULL;
int cache_last = 0;

/* print add linked list values*/
void print_list(node_t *main) 
{
	node_t *current = main;
	while (current != NULL) 
	{
		printf("%lld = %d \n", current->val, current->val2);
		current = current->next;
	}
}

/* add value to linked list */
void add(node_t *main, long long int val) 
{
	node_t *current = main;
	
	while (current->next != NULL) 
		current = current->next;
	
	cache_last++;
	current->next = malloc(sizeof(node_t));
	current->next->val = val;
	current->next->val2 = cache_last;
	current->next->next = NULL;
}

/* check if val exists in linked list */
int id_exists(node_t *main, long long int val)
{
	node_t *current = main;
	while (current->next != NULL) 
	{
		current = current->next;
		if (current->val == val)
			return 1;
	}
	return 0;
}

/* get val2 by searching val */
int get_user_id(node_t *main, long long int val)
{
	node_t *current = main;
	while (current->next != NULL) 
	{
		current = current->next;
		if (current->val == val)
			return current->val2;
	}
}

/* create new user in database */
void create_user(sqlite3 *db, char *username, int discriminator, long long int id, char *avatar)
{
	int rc;
	/* prepare sqlite statement */
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "INSERT INTO Users VALUES(?, ?, ?, ?, ?);", -1, &stmt, NULL);

	/* bind values to sqlite statement */
	sqlite3_bind_int(stmt, 1, get_user_id(cache, id));
	sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, discriminator);
	sqlite3_bind_int64(stmt, 4, id);
	sqlite3_bind_text(stmt, 5, avatar, -1, SQLITE_STATIC);

	/* execute statement */
	rc = sqlite3_step(stmt);

	/* check for errors */
	if (rc != SQLITE_DONE)
		printf("ERROR create_user inserting data: %s\n", sqlite3_errmsg(db));

	/* finalize our statement */
	sqlite3_finalize(stmt);
}

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
		 cJSON *author_id = cJSON_GetObjectItem(author, "id");
		 cJSON *avatar = cJSON_GetObjectItem(author, "avatar");

		/* update last_id */
		last_id.ptr = id->valuestring;

		/* test */
		if (id_exists(cache, strtoll(author_id->valuestring,NULL,0)) == 0)
		{
			add(cache, strtoll(author_id->valuestring,NULL,0));
			create_user(db,
				username->valuestring,
				atoi(discriminator->valuestring), /* atoi is workaround, valueint && valuedouble dont work correctly  */
				strtoll(author_id->valuestring, NULL, 0),  /* strtoll is workaround, valueint && valuedouble dont work correctly */
				avatar->valuestring);
			
		}
		
		/* if there is attachment set attachments->url and attachments->file_name*/
		if (cJSON_GetArraySize(attachments) != 0)
		{
			attachments_url = cJSON_GetObjectItem(attachments, "url");
			attachments_name = cJSON_GetObjectItem(attachments, "filename");
		}
			
		/* prepare sqlite statement */
		sqlite3_stmt *stmt;
		sqlite3_prepare_v2(db, "INSERT INTO Messages VALUES(?, ?, ?, ?, ?, ?);", -1, &stmt, NULL);
		//printf("%s#%s: %s\n", username->valuestring, discriminator->valuestring, content->valuestring); //debug

		/* bind values to sqlite statement */
		sqlite3_bind_int(stmt, 1, get_user_id(cache, strtoll(author_id->valuestring, NULL, 0))); /* atoi is workaround, valueint && valuedouble dont work correctly  */
		sqlite3_bind_int64(stmt, 2, strtoll(id->valuestring, NULL, 10)); /* strtoll is workaround, valueint && valuedouble dont work correctly */
		sqlite3_bind_text(stmt, 3, timestamp->valuestring, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 4, content->valuestring, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 5, attachments_url->valuestring, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 6, attachments_name->valuestring, -1, SQLITE_STATIC);
		
		/* execute statement */
		rc = sqlite3_step(stmt);

		/* check for errors */
		if (rc != SQLITE_DONE)
			printf("ERROR inserting data: %s\n", sqlite3_errmsg(db));

		/* finalize our statement */
		sqlite3_finalize(stmt);
		 
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
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0); /* ssl fix */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
	
	/* perform request and check for errors */
	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

	/* parse first message list */
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
	int rc = sqlite3_prepare(db, "Select Attachment_URL,Attachment_Filename,Id from Messages where NOT (Attachment_URL = 'NULL')", -1, &stmt, 0);

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
			return;

		/* set variables, note: outfilename needs unique identificator because files with same name can overwrite! */
		char *url = sqlite3_column_text(stmt, 0);
		char outfilename[512];
		
		/* change output directory if you are using unix */
		sprintf(outfilename, "C:\\download\\%s_%s", sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 1));
		
		/* check if photo exists */
		if (access(outfilename, F_OK) != -1) 
		{
			if (verbose == 1)
				printf("Found %s ,skipping.", outfilename);
		} 
		else
		{
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
			if (verbose == 1)
				printf("\n\nURL: '%s' \nFilename: '%s'", url, outfilename);

			/* evaluate our statement */
			rc = sqlite3_step(stmt);
		}
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
	
	/* check if token length is smaller than 64 */
	if (strlen(argv[2]) >= 64)
	{
		fprintf(stderr, "Incorrect token argument - token is too long.\n");
		return 8;
	}
	
	/* check if database name length is smaller than 159 */
	if (strlen(argv[3]) >= 159)
	{
		fprintf(stderr, "Incorrect database name argument - file name is too long.\n");
		return 9;
	}
	
	/* check argument only if argument isn't null */
	if (argv[4] != NULL)
		if (strcmp(argv[4], "1") == 0)
			verbose = 1;

	/* init cache (will fix later) */
	cache = malloc(sizeof(node_t));
	cache->val = 9996999;
	cache->val2 = 0;
	cache_last = 0;
	cache->next = NULL;
	
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
	
	/* faster but more unsafe */
	sqlite3_exec(db, "PRAGMA synchronous=OFF", 0, 0, 0);
	
	/* drop and create new table */
	rc = sqlite3_exec(db,
		"DROP TABLE IF EXISTS Messages;"
		"DROP TABLE IF EXISTS Users;"
		"CREATE TABLE Messages(UserId INTEGER, Id Bigint, Timestamp Text, Message Text, Attachment_URL Text, Attachment_Filename Text);"
		"CREATE TABLE Users(UserId INTEGER, Username Text, Discriminator Smallint, Id Bigint, Avatar Text);"
		, 0, 0, &err_msg);
	
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
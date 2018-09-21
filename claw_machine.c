#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
struct Customer {
	int id;
	/* round */
        int arrive;
        int cont; // continuously play round
        int rest;
	int total;
	int start; // should start
	int accum;
	int explored;
	int prewait;
        struct Customer *next;
};
static struct Customer *head = NULL;
static pthread_t threads[2];
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int G = 0, gcount = 0;
static int it = 1, threadnum, existcus = 0;
/*
<the guarantee number G>
<total number of customers>
<customer1 arrive time><continuously play round><rest time><total play round number N>
<customer2 arrive time><continuously play round><rest time><total play round number N>
*/
void save_file_content(char *filename) {
	char *line = NULL;
	size_t capacity = 0;
	ssize_t byte;
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		printf("file does not exist\n");
		exit(1);
	}
	byte = getline(&line, &capacity, fp);
	if (byte <= 0) {
		printf("error read file\n");
		exit(1);
	}
	// guarantee rounds
	G = atoi(line);
	byte = getline(&line, &capacity, fp);
	if (byte <= 0) {
		printf("error read file\n");
		exit(1);
	}
	int C = atoi(line);
	char *str;
	struct Customer *cus;
	for (int i = 0; i < C; i++) {
		byte = getline(&line, &capacity, fp);
		if (byte <= 0) {
			printf("error read file\n");
			exit(1);
		}
		cus = malloc(sizeof(struct Customer));
		cus->id = i + 1;
		str = strtok(line, " ");
		cus->arrive = atoi(str);
		str = strtok(NULL, " ");
		cus->cont = atoi(str);
		str = strtok(NULL, " ");
		cus->rest = atoi(str);
		str = strtok(NULL, " ");
		cus->total = atoi(str);
		cus->start = cus->arrive;
		cus->accum = 0;
		cus->prewait = 0;
		cus->explored = 0;
		cus->next = head;
		head = cus;
	}
	free(line);
	fclose(fp);
}

void eject(struct Customer *this) {
	struct Customer *prev, *cus = head;
	while (cus && cus != this) {
		prev = cus;
		cus = cus->next;
	}
	if (head == this) {
		head = this->next;
	} else {
		prev->next = this->next;
	}
}
void inject(struct Customer *this) {
	this->next = head;
	head = this;
}
void check_finish(int time, int id, struct Customer **customer) {
	(*customer)->accum++;
	gcount++;
	if (time >= (*customer)->start + (*customer)->cont || (*customer)->accum >= (*customer)->total || gcount >= G) {
		printf("%d %d finish playing", time, (*customer)->id);
		if ((*customer)->accum >= (*customer)->total || gcount >= G) {
			printf(" YES #%d\n", id);
			gcount = 0;
			free(*customer);
		} else {
			printf(" NO #%d\n", id);
			(*customer)->prewait = 0;
			(*customer)->explored = 0;
			(*customer)->start = time + (*customer)->rest;
			inject(*customer);
		}
		*customer = NULL;
		existcus--;
	}
}

struct Customer *get_customer(int time, int id) {
	struct Customer *cus = head, *result = NULL;
	int minstart = INT_MAX;
	while (cus) {
		if (cus->start <= time && cus->start < minstart) {
			result = cus;
			minstart = cus->start;
		}
		cus = cus->next;
	}
	if (result) {
		eject(result);
		printf("%d %d start playing #%d\n", time, result->id, id);
		result->start = time;
		existcus++;
		return result;
	} else {
		// both of machine are released
		if (existcus == 0) {
			gcount = 0;
		}
	}
	return NULL;
}

void check_waiting(int time, int id) {
	struct Customer *cus;
	cus = head;
	while (cus) {
		if (cus->start <= time) {
			// explored by other thread
			if (cus->explored >= 1 && cus->prewait == 0) {
				printf("%d %d wait in line\n", time, cus->id);
				cus->prewait = 1;
			} else {
				cus->explored++;
			}
		}
		cus = cus->next;
	}
}

int handle_customer(int time, int id, struct Customer **customer) {
	pthread_mutex_lock(&m);
	//printf("gcount=%d, G=%d\n", gcount, G);
	while(it != id){
		pthread_cond_wait(&cv, &m);
	}
	// handle existing customer
	if (*customer) {
		check_finish(time, id, customer);
	}
	// no existing customer
	if (*customer == NULL) {
		// no more customer
		if (head == NULL) {
			threadnum--;
			// change to other thread
			it = it % 2 + 1;
			pthread_mutex_unlock(&m);
			pthread_cond_signal(&cv);
			return 0;
		}
		*customer = get_customer(time, id);
	}
	check_waiting(time, id);
	if (threadnum == 2) {
		// change to other thread
		it = it % 2 + 1;
	}
	pthread_mutex_unlock(&m);
	pthread_cond_signal(&cv);
	return 1;
}

void *claw_machine(void *arg) {
	int id = *(int *)arg;
	int time = 0;
	struct Customer* cus = NULL;
	while (handle_customer(time, id, &cus)) {
		time++;
	}
	return NULL;
}
int main(int argc, char **argv) {
	if (argc != 2) {
		printf("usage: ./<exec file> <input file>\n");
		exit(1);
	}
	save_file_content(argv[1]);
	int t0 = 1, t1 = 2;
	threadnum = 2;
	pthread_create(&threads[0], NULL, claw_machine, &t0);
	pthread_create(&threads[1], NULL, claw_machine, &t1);
	for (int i = 0; i < 2 ; i++) {
		pthread_join(threads[i], NULL);
	}
}

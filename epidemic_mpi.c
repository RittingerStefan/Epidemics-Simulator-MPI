#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "mpi.h"

// #define DEBUG

#define NANO 1000000000.0

#define CARDINAL_N 0
#define CARDINAL_S 1
#define CARDINAL_E 2
#define CARDINAL_W 3

#define DIR_VERTICAL 0
#define DIR_HORIZONTAL 1

#define STAT_INFECTED 0
#define STAT_SUSCEPTIBLE 1
#define STAT_IMMUNE 2

#define TIME_INFECTED 3
#define TIME_IMMUNE 3

#define BUFFER_SIZE 100

typedef struct person_t {
    int id;
    int x, y;
    int movement_pattern, amplitude;
    int status, got_infected;
    int timer_infected, timer_immune;
    int count_infected;
} person_t;

int simulation_time;
FILE* input_file = NULL;
char* file_name;
int max_coord_x, max_coord_y;
int people_number;
person_t *people_serial, *people_mpi, *recv;
MPI_Datatype MPI_Person;

person_t generate_person(int id, int x, int y, int init_status, int pattern, int amplitude) {
    person_t person;
    
    person.id = id;
    if(x < 0 || y < 0 || x > max_coord_x || y > max_coord_y) {
        printf("Coordinates are out of bounds.\n");
        exit(-6);
    }
    person.x = x; person.y = y;

    switch(pattern) {
        case CARDINAL_N:
            person.movement_pattern = DIR_VERTICAL; person.amplitude = -1 * amplitude;
            break;
        case CARDINAL_S:
            person.movement_pattern = DIR_VERTICAL; person.amplitude = amplitude;
            break;
        case CARDINAL_E:
            person.movement_pattern = DIR_HORIZONTAL; person.amplitude = amplitude;
            break;
        case CARDINAL_W:
            person.movement_pattern = DIR_HORIZONTAL; person.amplitude = -1 * amplitude;
            break;
        default:
            printf("Undefined movement pattern.\n");
            exit(-6);
    }

    person.status = init_status;
    person.timer_infected = TIME_INFECTED; person.timer_immune = TIME_IMMUNE;
    person.count_infected = 0;
    person.got_infected = 0;

    return person;
}

void handle_arguments(char* argv[]) {
    simulation_time = atoi(argv[1]);
    if(simulation_time <= 0) {
        printf("Incorrect simulation time value.\n");
        exit(-2);
    }

    input_file = fopen(argv[2], "r");
    if(input_file == NULL) {
        printf("Error opening the file.\n");
        exit(-2);
    }
    file_name = malloc(sizeof(char) * strlen(argv[2]));
    strncpy(file_name, argv[2], strlen(argv[2]));
}

person_t get_person_data_from_string(char* string, int line) {
    char* strtok_pointer;
    int id, x, y, status, pattern, amplitude;
    errno = 0;
    strtok_pointer = strtok(string, " "); id = atoi(strtok_pointer);
    strtok_pointer = strtok(NULL, " "); x = strtod(strtok_pointer, NULL);
    strtok_pointer = strtok(NULL, " "); y = strtod(strtok_pointer, NULL);
    strtok_pointer = strtok(NULL, " "); status = strtod(strtok_pointer, NULL);
    strtok_pointer = strtok(NULL, " "); pattern = strtod(strtok_pointer, NULL);
    strtok_pointer = strtok(NULL, " "); amplitude = atoi(strtok_pointer);

    if(id <= 0 || amplitude <= 0 || errno != 0) {
        printf("Error parsing person data at line: %d\n", line);
        exit(-4);
    }

    return generate_person(id, x, y, status, pattern, amplitude);
}

void read_input_from_file() {
    if(input_file == NULL) {
        printf("No file was opened! Cannot read data.\n");
        exit(-3);
    }

    char* buffer = malloc(BUFFER_SIZE * sizeof(char));
    char* buffer_copy = malloc(BUFFER_SIZE * sizeof(char));
    char* strtok_pointer;

    fgets(buffer, BUFFER_SIZE, input_file);
    strtok_pointer = strtok(buffer, " "); max_coord_x = atoi(strtok_pointer);
    strtok_pointer = strtok(NULL, " "); max_coord_y = atoi(strtok_pointer);
    if(max_coord_x <= 0 || max_coord_y <= 0) {
        printf("Error reading the max coordinates.\n");
        exit(-3);
    }

    fgets(buffer, BUFFER_SIZE, input_file);
    people_number = atoi(buffer);
    if(people_number <= 0) {
        printf("Error reading the number of people.\n");
        exit(-3);
    }

    people_serial = malloc(people_number * sizeof(person_t));
    people_mpi = malloc(people_number * sizeof(person_t));
    for(int i = 0; i < people_number; i++) {
        fgets(buffer, BUFFER_SIZE, input_file);
        strncpy(buffer_copy, buffer, strlen(buffer));
        people_serial[i] = get_person_data_from_string(buffer, i);
        people_mpi[i] = get_person_data_from_string(buffer_copy, i);
    }

    free(buffer);
    free(buffer_copy);
}

void cleanup() {
    fclose(input_file);
    free(file_name);
    free(people_serial);
    free(recv);
}

void update_position(person_t* person) {
    int new_x = person->x;
    int new_y = person->y;
    int amplitude = person->amplitude;

    if(person->movement_pattern == DIR_VERTICAL) new_y += amplitude;
    else new_x += amplitude;

    if(new_y < 0) {
        new_y = 0 ;
        amplitude *= -1;
    }

    if(new_y >= max_coord_y) {
        new_y = max_coord_y - 1;
        amplitude *= -1;
    }

    if(new_x < 0) {
        new_x = 0;
        amplitude *= -1;
    }

    if(new_x > max_coord_x) {
        new_x = max_coord_x - 1;
        amplitude *= -1;
    }

    person->x = new_x;
    person->y = new_y;
    person->amplitude = amplitude;
}

void infect_neighbors(person_t infected_person, person_t* people) {
    for(int i = 0; i < people_number; i++)
        if(people[i].x == infected_person.x && people[i].y == infected_person.y 
           && people[i].id != infected_person.id && people[i].status == STAT_SUSCEPTIBLE)
                people[i].got_infected = 1;
}

void set_next_status(person_t* person) {
    if(person->status == STAT_SUSCEPTIBLE && person->got_infected) {
        person->status = STAT_INFECTED;
        person->timer_infected = TIME_INFECTED;
        person->count_infected++;
        person->got_infected = 0; 
    } else if(person->status == STAT_INFECTED) {
        person->timer_infected--;
        if(person->timer_infected == 0) {
            person->status = STAT_IMMUNE;
            person->timer_immune = TIME_IMMUNE;
        }
    } else if(person->status == STAT_IMMUNE) {
        person->timer_immune--;
        if(person->timer_immune == 0)
            person->status = STAT_SUSCEPTIBLE;
    }    
}

void print_person_data(person_t person) {
    char status[15] = "";

    switch(person.status) {
        case STAT_SUSCEPTIBLE:
            strcpy(status, "SUSCEPTIBLE");
            break;
        case STAT_INFECTED:
            strcpy(status, "INFECTED");
            break;
        case STAT_IMMUNE:
            strcpy(status, "IMMUNE");
            break;
    }

    printf("Person %d: (%d, %d), status: %s, was infected %d time(s).\n", person.id, person.x, person.y, status, person.count_infected);
}

void write_result_in_file(char* append, person_t* people) {
    char* file_name_copy = malloc(sizeof(char) * strlen(file_name));
    strncpy(file_name_copy, file_name, strlen(file_name));

    char* file_name_no_extension = strtok(file_name_copy, ".");
    char* new_file_name = malloc(sizeof(char) * (strlen(file_name_no_extension) + strlen(append)));
    char status[15] = "";
    FILE* write_file;

    strcpy(new_file_name, file_name_no_extension);
    strcat(new_file_name, append);
    write_file = fopen(new_file_name, "w");

    if(write_file == NULL) {
        printf("Error creating output file.\n");
        return;
    }

    for(int i = 0; i < people_number; i++) {
        switch(people[i].status) {
            case STAT_SUSCEPTIBLE:
                strcpy(status, "SUSCEPTIBLE");
                break;
            case STAT_INFECTED:
                strcpy(status, "INFECTED");
                break;
            case STAT_IMMUNE:
                strcpy(status, "IMMUNE");
                break;
        }
        fprintf(write_file, "Person %d: (%d, %d), status: %s, was infected %d time(s).\n", people[i].id, people[i].x, people[i].y, status, people[i].count_infected);
    }
    printf("Results printed in file: %s\n", new_file_name);
    free(new_file_name);
    free(file_name_copy);
    fclose(write_file);
}

int check_equal(person_t p1, person_t p2) {
    if(p1.id != p2.id) return 0;
    if(p1.x != p2.x) return 0;
    if(p1.y != p2.y) return 0;
    if(p1.status != p2.status) return 0;
    if(p1.count_infected != p2.count_infected) return 0;

    return 1;
}

int check_if_same_result() {
    for(int i = 0; i < people_number; i++)
        if(!check_equal(people_mpi[i], people_serial[i])) {
            return i + 1;
        }
    return -1;
}


void epidemic_simulation_serial() {
    for(int time = 0; time < simulation_time; time++) {
        
        for(int i = 0; i < people_number; i++) 
            update_position(&people_serial[i]);

        for(int i = 0; i < people_number; i++)
            if(people_serial[i].status == STAT_INFECTED)
                infect_neighbors(people_serial[i], people_serial);

        for(int i = 0; i < people_number; i++) 
            set_next_status(&people_serial[i]);

#ifdef DEBUG
        for(int i = 0; i < people_number; i++)
            print_person_data(people_serial[i]);
        printf("\n");
#endif
    }
}

void epidemic_simulation_mpi(int comm_size, int rank) {

    int chunk_size = people_number / comm_size;
    person_t* chunk = (person_t*)malloc(chunk_size * sizeof(person_t));
    recv = NULL;

    for(int i = 0; i < simulation_time; i++) {
        
        if(rank == 0) {
            if(recv == NULL) {
                recv = (person_t*)malloc(people_number * sizeof(person_t));
            }
            
            if(recv == NULL) {
                printf("Error creating receive buffer.\n");
                return;
            }
        }

        MPI_Scatter(people_mpi, chunk_size, MPI_Person, chunk, chunk_size, MPI_Person, 0, MPI_COMM_WORLD);
        for(int i = 0; i < chunk_size; i++) {
            update_position(&chunk[i]);
        }
        MPI_Gather(chunk, chunk_size, MPI_Person, recv, chunk_size, MPI_Person, 0, MPI_COMM_WORLD);

        if(rank == 0) {
            free(people_mpi);
            people_mpi = recv;
            for(int i = 0; i < people_number; i++)
                if(people_mpi[i].status == STAT_INFECTED)
                    infect_neighbors(people_mpi[i], people_mpi);
        }

        MPI_Scatter(people_mpi, chunk_size, MPI_Person, chunk, chunk_size, MPI_Person, 0, MPI_COMM_WORLD);
        for(int i = 0; i < chunk_size; i++) {
            set_next_status(&chunk[i]);
        }
        MPI_Gather(chunk, chunk_size, MPI_Person, recv, chunk_size, MPI_Person, 0, MPI_COMM_WORLD);

        if(rank == 0) {
            people_mpi = recv;
        }
    }
    free(chunk);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("Please provide the following arguments: simulation time, input file name.\n");
        return -1;
    }

    struct timespec start;
    struct timespec end;
    int comm_size, my_rank;
    double time_serial, time_mpi;

    MPI_Init(&argc, &argv);
    MPI_Type_contiguous(10, MPI_INT, &MPI_Person);
    MPI_Type_commit(&MPI_Person);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if(my_rank == 0) {
        handle_arguments(argv);
        read_input_from_file();

        if(people_number % comm_size != 0) {
            printf("Number of processes is not divisible with number of people!\n");
            cleanup();
            return -1;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        epidemic_simulation_serial();
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_serial = end.tv_sec - start.tv_sec;
        time_serial += (end.tv_nsec - start.tv_nsec) / NANO;
        printf("Time for serial: %lf\n", time_serial);
        write_result_in_file("_serial_out.txt", people_serial);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Bcast(&people_number, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&simulation_time, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&max_coord_x, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&max_coord_y, 1, MPI_INT, 0, MPI_COMM_WORLD);

    clock_gettime(CLOCK_MONOTONIC, &start);
    epidemic_simulation_mpi(comm_size, my_rank);
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_mpi = end.tv_sec - start.tv_sec;
    time_mpi += (end.tv_nsec - start.tv_nsec) / NANO;
    MPI_Barrier(MPI_COMM_WORLD);
    
    if(my_rank == 0) {
        printf("Time for mpi parallel: %lf\n", time_mpi);
        write_result_in_file("_mpi_out.txt", people_mpi);

        double speedup = time_serial / time_mpi;
        int error_index = check_if_same_result();

        if(error_index > 0) {
            printf("!!! RESULTS DO NOT MATCH AT INDEX %d !!!\n", error_index);
        }

        printf("\nMeasured Speedup: %lf\n", speedup);
        printf("Measured efficiency: %lf\n", speedup / comm_size);
    }

    MPI_Finalize();
    if(my_rank == 0) {
        cleanup();
    }
    return 0;
}
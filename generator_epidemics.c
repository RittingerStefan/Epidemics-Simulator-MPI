/**
 * Helper program to generate input data files for the Epidemics Simulation progaram
 * Generates files containing initial data for N random generated persons
 * Accepts as input:
 * Size  of grid maxX*maxY points
 * Number of persons N - give a reasonable value for N relative to the number of grid points
 * infectionPercentage - the percentage of persons initially infected 
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct
{
    int personID;
    int x;
    int y;
    int status;    // 0=infected, 1=susceptible
    int direction; // 0: N, 1: S, 2: E, 3: W
    int amplitude;
} Person;

void generatePersons(const char *filename, int N, int maxX, int maxY, int infectionPercentage)
{
    FILE *file = fopen(filename, "w");
    if (!file)
    {
        printf("Error opening file\n");
        exit(1);
    }

    // Calculate the number of persons that are initially infected
    int infectedCount = (N * infectionPercentage) / 100;
    if (infectedCount < 1)
        infectedCount = 1; // Ensure at least one infected person

    // generate the right number of infection statuses
    int *statuses = malloc(N * sizeof(int));
    for (int i = 0; i < infectedCount; i++)
    {
        statuses[i] = 0; // infected person
    }
    for (int i = infectedCount; i < N; i++)
    {
        statuses[i] = 1; // susceptible person
    }

    // shuffle statuses array for randomization
    for (int i = N - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        int temp = statuses[i];
        statuses[i] = statuses[j];
        statuses[j] = temp;
    }

    
    fprintf(file, "%d %d\n", maxX, maxY); // grid sizes 

    fprintf(file, "%d\n", N); // number of persons
    for (int i = 0; i < N; i++)
    {
        Person p;
        p.personID = i + 1;
        p.x = rand() % (maxX - 1); // x coordinate
        p.y = rand() % (maxY - 1); // y coordinate
        p.status = statuses[i];    // status from randomized array

        // movement direction and amplitude
        p.direction = rand() % 4;                     // direction (0=N 1=S 2=E 3=W)
        if ((p.direction == 0) || (p.direction == 1)) // N,S
            p.amplitude = 1 + rand() % (maxY / 2);  //movement amplitude must be at least 1
        else if ((p.direction == 2) || (p.direction == 3)) // E, W
            p.amplitude = 1 + rand() % (maxX / 2);

        // write the data of one person on one line
        fprintf(file, "%d %d %d %d %d %d\n",
                p.personID, p.x, p.y, p.status, p.direction, p.amplitude);
    }

    free(statuses);
    fclose(file);
}

int main()
{
    srand(time(NULL));
    int N, maxX, maxY, infectionPercentage;

    printf("Enter value for max X coordinate: ");
    scanf("%d", &maxX);
    printf("Enter value for max Y coordinate: ");
    scanf("%d", &maxY);

    printf("On your grid there are %d points \n", maxX * maxY);

    printf("Enter the number of persons: ");
    scanf("%d", &N);

    if (N <= 0)
    {
        printf("The number of persons must be >=0 \n");
        exit(1);
    }

    printf("Enter the percentage of initially infected persons (0-100): ");
    scanf("%d", &infectionPercentage);

    if (infectionPercentage < 0 || infectionPercentage > 100)
    {
        printf("Infection percentage must be between 0 and 100 !\n");
        exit(1);
    }

    // Create the output file name 
    char filename[30];
    if (N >= 1000000)
    {
        int millions = N / 1000000;
        snprintf(filename, sizeof(filename), "epidemics%dM.txt", millions);
    }
    else if (N >= 1000)
    {
        int thousends = N / 1000;
        snprintf(filename, sizeof(filename), "epidemics%dK.txt", thousends);
    }
    else
    {
        snprintf(filename, sizeof(filename), "epidemics%d.txt", N);
    }

    generatePersons(filename, N, maxX, maxY, infectionPercentage);
    printf("Simulation data generated in file %s\n", filename);

    return 0;
}

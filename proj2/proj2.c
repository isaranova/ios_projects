/**
 * IOS PROJEKT 2
 *
 * The senate bus problem solution inspired by algorithm in Little book of semaphores
 *
 * @author Ivana Saranova (xsaran02)
 * @date 30.4.2018
 *
 * @see http://greenteapress.com/semaphores/LittleBookOfSemaphores.pdf
 */

/* libraries needed for the project*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>

/***************************************
 * Declaration of all global variables
 */

/* semaphores and varibles needed for senate-bus algorithm according to Little book of semaphores*/
sem_t *mutex = NULL; //protection for waiting variable
sem_t *sem_bus = NULL; //semaphore for signaling that bus has arrived
sem_t *boarded = NULL; //semaphore for signaling that rider has boarded the bus
int *waiting = NULL; //the number of riders at the buss stop

/* project arguments global variables declaration */
int r; //the number of rider that will be generated
int c; //the number of maximum boarded riders on bus
int art; //the number for random time number between each rider process in interval <0, art>
int abt; //the number for random time number simulating bus ride in interval <0, abt>

/* help semaphores used for writing into file and finishing riders after bus ends */
sem_t *file_write = NULL; //protection for writing into file
sem_t *end_bus = NULL; //for signaling that bus has ended

/* variable Id for shared memory usage */
int waitingId = 0;

/* File variable for the proj2.out and its Id for shared memory usage */
FILE **file = NULL;
int fileId = 0;

/* Action variable for numbering the actions written to output file and its Id for shared memory usage */
int turnId = 0;
int *turn = NULL;

/***************************************
 * Function prototypes
 */
void bus();
void rider();
void board();
void bus_print_msg(char *msg, int action, int wait, int numb);
void rider_print_msg(char *msg, int action, int id, int wait, int numb);

int check_boundaries_gt(int numb, int gt);
int check_boundaries_ge_le(int numb, int ge, int le);
int check_arguments(char **argv, int argc);
int error_print(char **argv, int argc);
void semaphore_des(sem_t *sem, char *name);
int memory_init();
void memory_des();
void semaphores_des();

/**************************MAIN*****************************/

int main(int argc, char **argv)
{
    setbuf(stdout, NULL); //setting the stdout buffer to null
    srand(time(NULL)); //setting the srand function

    //checking for errors in arguments
    if(error_print(argv, argc) == -1)
    {
        return -1;
    }

    //initializing shared memory variables and checking for errors
    int nmem = memory_init();
    if (nmem == -1)
    {
        memory_des();
        return -1;
    }

    //setting the turn variable to 1
    *turn = 1;

    //opening the output file for writing and checking for errors
    *file = fopen("proj2.out", "w");
    if(file == NULL)
    {
        fprintf(stderr, "Error - proj2.out could not be opened\n");
        return -1;
    }
    setbuf(*file, NULL); //setting the file buffer to null

    //setting the arguments into variables
    char *ptr;
    r = strtol(argv[1], &ptr, 10);
    c = strtol(argv[2], &ptr, 10);
    art = strtol(argv[3], &ptr, 10);
    abt = strtol(argv[4], &ptr, 10);

    //creating the semaphores and checking for errors
    if((mutex = sem_open("/mutex_sem", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED
       || (sem_bus = sem_open("/sem_bus", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED
       || (boarded = sem_open("/boarded", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED
       || (file_write = sem_open("/file_write", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED
       || (end_bus = sem_open("/end_bus", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED)
    {
        fprintf(stderr, "Error - Semaphore could not be created\n");
        semaphores_des();
        memory_des();
        fclose(*file);
        return -1;
    }

    //PROCESS PART//
    //declaring the main process and the riders generator process
    pid_t help_riders;
    pid_t main_process = fork(); //forking the main_process

    if(main_process == 0) //child = bus process
    {
        bus();
        exit(0);
    }
    else if(main_process > 0)
    {
        //parent
    }
    else //error handling
    {
        fprintf(stderr, "Error - process could not be created\n");
        semaphores_des();
        memory_des();
        fclose(*file);
        return -1;
    }

    help_riders = fork(); //forking the riders generator process
    if(help_riders == 0) //child = rider processes
    {
        int random_time; //variable for storing the random time between each rider
        pid_t rider_process; //rider process
        //for cycle generating the rider processes defined by number stored in global variable r
        for(int i = 1; i < r +1; i++)
        {
            random_time=rand() % (art+1) * 1000; //generating the random time number between each processes
            usleep(random_time); //waiting until new process is created
            rider_process = fork(); //forking the rider process
            if(rider_process == 0) //child = one rider
            {
                rider(i);
            }
            else if(rider_process > 0)
            {
                //parent
            }
            else //error handling
            {
                fprintf(stderr, "Error - process could not be created\n");
                semaphores_des();
                memory_des();
                fclose(*file);
                return -1;
            }

        }

        waitpid(rider_process, NULL, 0); //waiting for all the riders to finish
        exit(0); //destroying the rider process
    }
    else if(help_riders > 0) //parent
    {
    }
    else //error
    {
        fprintf(stderr, "Error - process could not be created\n");
        semaphores_des();
        memory_des();
        fclose(*file);
        return -1;
    }

    waitpid(help_riders, NULL, 0); //waiting for riders generator to end
    waitpid(main_process, NULL, 0); //waiting for main process to end

    semaphores_des(); //destroy the semaphores
    memory_des(); //destroy the shared memory variables
    fclose(*file); //close the file

    return 0;
}


/***********************************
 * Function
 */

/**
 * Checks if number is greater than another number
 *
 * @param numb Number to be compared with limit number
 * @param gt Limit number
 *
 * @return When the number is greater than the limit number the function returns 1, else it returns 0.
 */
int check_boundaries_gt(int numb, int gt)
{
    if(numb <= gt)
    {
        return 0;
    }
    return 1;
}

/**
 * Checks if number is greater or equal than one number and lesser or equal than another number
 *
 * @param numb Number to be compared with limit numbers
 * @param ge Limit number one
 * @param le Limit number two
 *
 * @return When the number is greater or equal than the limit number one and lesser or equal than
 * the limit number two the function returns 1, else it returns 0.
 */
int check_boundaries_ge_le(int numb, int ge, int le)
{
    if(numb < ge || numb > le)
    {
        return 0;
    }
    return 1;
}

/**
 * Checks if there is enough arguments, if the arguments are numbers and integers and if the numbers are in
 * the right intervals
 *
 * @brief Checks if arguments are in proper format
 *
 * @param argv Array of program's arguments
 * @param argc Number of program's arguments
 *
 * @return If there isn't exactly 5 arguments, function returns 1. If one of the arguments isn't number,
 * function returns 2. If one of the arguments isn't integer, function returns 3. If one of the first two
 * arguments isn't greater than 0, function returns 4. If one of the last two arguments isn't greater or equal
 * to 0 and lesser or equal to 1000, function returns 5. Else if there aren't any mistakes in argument format,
 * function return 0.
 */
int check_arguments(char **argv, int argc)
{
    if(argc != 5)
    {
        return 1; //chyba pocet argumentu
    }

    for(int i = 1; i < argc; i++)
    {
        char *ptr;
        int inumb = strtol(argv[i], &ptr, 10);

        if(ptr[0] == '.')
        {
            return 3; //chyba neni int
        }

        if(*ptr != '\0')
        {
            return 2; //chyba, neni cislo
        }

        if(i < 3)
        {
            if(check_boundaries_gt(inumb, 0) == 0)
            {
                return 4; //chyba neni v rozmezi
            }
        }

        if(i > 3)
        {
            if(check_boundaries_ge_le(inumb, 0, 1000) == 0)
            {
                return 4;
            }
        }
    }

    return 0;
}

/**
 * Prints error messages for wrong argument formats
 *
 * @param argv Array of program's arguments
 * @param argc Number of program's arguments
 *
 * @return If there is any problem with the format of arguments, function prints error message to stderr and return -1.
 * Else function returns 0.
 */
int error_print(char **argv, int argc)
{
    int errtype = check_arguments(argv, argc);
    switch(errtype)
    {
        case 1:
            fprintf(stderr,"Error - Wrong number of arguments\n");
            return -1;
        case 2:
            fprintf(stderr,"Error - Arguments must be numbers\n");
            return -1;
        case 3:
            fprintf(stderr,"Error - Arguments must be integers\n");
            return -1;
        case 4:
            fprintf(stderr,"Error - First two arguments must be greater than zero, last two arguments must be greater or equal to zero and lesser or equal to 1000\n");
            return -1;
        default:
            return 0;
    }
}

/**
 * Prints messages to the output file for the bus function
 *
 * @param msg String containing the message
 * @param action Number of current action
 * @param wait Number of riders waiting at the bus stop
 * @param numb Number of arguments for the message
 */
void bus_print_msg(char *msg, int action, int wait, int numb)
{
    sem_wait(file_write);
    if(numb == 1)
        fprintf(*file, msg, action);
    else
        fprintf(*file, msg, action, wait);
    (*turn)++; //raising the action number
    sem_post(file_write);
}


/**
 * Prints messages to the output file for the rider function
 *
 * @param msg String containing the message
 * @param action Number of current action
 * @param wait Number of riders waiting at the bus stop
 * @param numb Number of arguments for the message
 */
void rider_print_msg(char *msg, int action, int id, int wait, int numb)
{
    sem_wait(file_write);
    if(numb == 2)
        fprintf(*file, msg, action, id);
    else
    {
        fprintf(*file, msg, action, id, wait);
        (*waiting)++; //increasing the number of riders waiting at the bus stop
    }
    (*turn)++; //raising the action number
    sem_post(file_write);
}

/**
 * Function simulating bus. Bus starts with "start" and ends with "finish". The number of bus routes is
 * defined by number of boarded riders. Bus starts each route with "arrival", when its arriving to
 * the bus stop, and ends it with "end". When bus is at the bus stop, it prints message "start boarding"
 * and all riders waiting at the bus stop board the bus. The number of riders that can board the bus is defined
 * by the number in the global variable c. After all the riders are boarded, the bus prints message "end boarding"
 * and then leaves the bus stop with "depart". After depart, it simulates driving for a random time in interval <0, abt>.
 *
 * @brief Function simulating bus
 */
void bus()
{
    int boarded_riders = 0; //local variable, stores the number of already boarded riders

    //writing to file, that bus starts
    bus_print_msg("%d:	BUS:	start\n", *turn, 0, 1);

    //while cycle simulating bus riding in several routes until all riders are served
    while(boarded_riders < r)
    {
        sem_wait(mutex);

        //writing to file, that bus arrived to the bus stop
        bus_print_msg("%d:	BUS:	arrival\n", *turn, 0, 1);

        int n; //variable storing the number of riders, that will be boarded
        //the maximum of boarded riders is stored in global variable c, if there are more riders on the bus stop
        //then bus can take, only the maximum number in c of riders will be boarded
        if(*waiting <= c)
        {
            n = *waiting;
        }
        else n = c;

        //bus starts boarding the riders only if there are any waiting on the bus stop
        if(n > 0)
        {
            //writing to file, that bus starts boarding
            bus_print_msg("%d:	BUS:	start boarding:	%d\n", *turn, *waiting, 2);

            //boarding each rider specially
            for(int i = 0; i < n; i++)
            {
                sem_post(sem_bus); //signaling that a rider can be boarded
                sem_wait(boarded); //waiting until a single rider is boarded
            }

            //changing the number of riders waiting at the bus stop
            if(*waiting-c >= 0)
            {
                *waiting = *waiting-c;
            }
            else *waiting = 0;

            //adding all the boarded riders to the number of already boarded riders
            boarded_riders += n;

            //writing to file, that bus has stopped boarding
            bus_print_msg("%d:	BUS:	end boarding:	%d\n", *turn, *waiting, 2);
        }

        //writing to file, that bus has departed from the bus stop
        bus_print_msg("%d:	BUS:	depart\n", *turn, 0, 1);

        sem_post(mutex);

        //simulating the bus ride for random time in interval <0, abt>
        //only if the abt number is greater than 0
        if(abt > 0)
        {
            int random_time_bus = rand() % (abt+1) * 1000;
            usleep(random_time_bus);
        }

        //writing to file, that bus ride has ended and bus shall start a new route
        bus_print_msg("%d:	BUS:	end\n", *turn, 0, 1);

        //letting of all the riders boarded off the bus
        for(int i = 0; i < n; i++)
        {
            sem_post(end_bus); //signaling that rider can finish
        }
    }

    //writing to file, that bus has finished
    bus_print_msg("%d:	BUS:	finish\n", *turn, 0, 1);
}

/**
 * Function simulating a single rider. When rider is created, function prints to the output file "start", when rider
 * is destroyed (gets off the bus), function prints "finish". After rider is created, it enters the bus stop
 * with message "enter", which also states the number or riders already waiting at the bus stop including the newcome.
 * After the bus arrives to the bus stop, riders are boarding the bus one after another.
 *
 * @brief Function simulating the rider
 *
 * @param id ID number of the rider
 */
void rider(int id)
{
    //writing message to file, that new rider has been created
    rider_print_msg("%d:	RID %d:	start\n", *turn, id, 0, 2);

    sem_wait(mutex);

    //writing message to file, that rider has entered the bus stop
    rider_print_msg("%d:	RID %d:	enter:  %d\n", *turn, id, *waiting+1, 3);

    sem_post(mutex);

    //waiting for the signal, that bus can be boarded
    sem_wait(sem_bus);
    board(id); //writing to file, that rider has boarded the bus
    sem_post(boarded); //signaling that rider has boarded the bus

    sem_wait(end_bus); //waiting until bus has ended its route
    //writing to file, that rider has been destroyed (finished)
    rider_print_msg("%d:	RID %d:	finish\n", *turn, id, 0, 2);

    exit(0); //ending the rider process
}

/**
 * Printing out message, that the rider has boarded the bus
 *
 * @param id ID number of the rider
 */
void board(int id)
{
    sem_wait(file_write);
    fprintf(*file, "%d:	RID %d:	boarding\n", *turn, id);
    (*turn)++; //increasing the number of action
    sem_post(file_write);
}

/**
 * Closing and unlinking a semaphore
 *
 * @param sem A semaphore to be closed
 * @param name A semaphore name to be unlinked
 */
void semaphore_des(sem_t *sem, char *name)
{
    sem_close(sem);
    sem_unlink(name);
}

/**
 * Creating a shared memory variables
 *
 * @return If all shared memory IDs and variables have been created successfully, function return 0,
 * else function returns -1
 */
int memory_init()
{
    if ((waitingId = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) == -1)
    {
        fprintf(stderr, "Error - memory id could not be obtained\n");
        return -1;
    }

    if ((waiting = shmat(waitingId, NULL, 0)) == NULL)
    {
        fprintf(stderr, "Error - memory variable could not be created\n");
        return -1;
    }

    if ((turnId = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) == -1)
    {
        fprintf(stderr, "Error - memory id could not be obtained\n");
        return -1;
    }

    if ((turn = shmat(turnId, NULL, 0)) == NULL)
    {
        fprintf(stderr, "Error - memory variable could not be created\n");
        return -1;
    }

    if ((fileId = shmget(IPC_PRIVATE, sizeof(FILE *), IPC_CREAT | 0666)) == -1)
    {
        fprintf(stderr, "Error - memory id could not be obtained\n");
        return -1;
    }

    if ((file = shmat(fileId, NULL, 0)) == NULL)
    {
        fprintf(stderr, "Error - memory variable could not be created\n");
        return -1;
    }

    return 0;
}

/**
 * Destroying shared memory variables
 */
void memory_des()
{
    if(waiting != NULL)
        shmctl(waitingId, IPC_RMID, NULL);
    if(file != NULL)
        shmctl(fileId, IPC_RMID, NULL);
    if(turn != NULL)
        shmctl(turnId, IPC_RMID, NULL);
}

/**
 * Destroying all semaphores
 */
void semaphores_des()
{
    if(mutex != NULL)
        semaphore_des(mutex, "/mutex_sem");
    if(sem_bus != NULL)
        semaphore_des(sem_bus, "/sem_bus");
    if(boarded != NULL)
        semaphore_des(boarded, "/boarded");
    if(file_write != NULL)
        semaphore_des(file_write, "/file_write");
    if(end_bus != NULL)
        semaphore_des(end_bus, "/end_bus");
}

/*************************END OF FILE*******************************/

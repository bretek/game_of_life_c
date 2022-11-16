#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <pthread.h>

/*make viewport (grid)

//get input using UI

//convert to cell items in dynamic linked list

//Split into 2 threads:
    print viewport
    check for input and print again if changed

    update dynamic linked list
*/

typedef struct Cells {
    int x;
    int y;
    int toRemove;
    struct Cells *last;
    struct Cells *next;
} cell;

struct List {
    cell *first;
    int size;
};

int update = 1;
//track population and generation
int pop = 0;
int gen = 0;

struct winsize win;

pthread_mutex_t mutex;

void printGrid(int gridWidth, int gridHeight, char grid[gridWidth][gridHeight]) {
    for (int row = 0; row < gridHeight; ++row) {
        for (int col = 0; col < gridWidth; ++col) {
            putc(grid[col][row], stdout);
        }
        putc('\n', stdout);
    }
}

void printDynamicGrid(int gridWidth, int gridHeight, char **grid) {
    for (int row = 0; row < gridHeight; ++row) {
        for (int col = 0; col < gridWidth; ++col) {
            putc(grid[col][row], stdout);
        }
        putc('\n', stdout);
    }
}

void updateGrid(int gridWidth, int gridHeight, char **grid, int centerX, int centerY, struct List *list) { 
    //init to all spaces
    for (int row = 0; row < gridHeight; ++row) {
        for (int col = 0; col < gridWidth; ++col) {
            grid[col][row] = ' ';
        }
    }
    
    //check all cells in list and add to grid
    cell *current = list->first;
    for (int item = 0; item < list->size; ++item) {
        //if in current viewport
        if (current->x < centerX+gridWidth && current->x >= centerX && current->y < centerY+gridHeight && current->y >= centerY) {
            //absolute x and y mapped to viewport x and y
            grid[current->x-centerX][current->y-centerY] = '#';
        }
        current = current->next;
    }
    
    printf("Generation: %d Population: %d \n", gen, pop);
    printDynamicGrid(gridWidth, gridHeight, grid);
    puts("a: left d:right w:up s:down enter:pause q:quit");
}

struct termios originalSettings;

void restoreTerminalSettings() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalSettings);
}

void changeTerminalSettings() {
    tcgetattr(STDIN_FILENO, &originalSettings);
    atexit(restoreTerminalSettings);
    
    struct termios settings = originalSettings;
    settings.c_lflag &= ~(ECHO | ICANON);
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 1;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &settings);
}

void moveAxis(int gridWidth, int gridHeight, char grid[gridWidth][gridHeight], int *cursorX, int *cursorY, int axis, int direction, int *marked) {
    update = 1;
    if (*marked == 0) {
        grid[*cursorX][*cursorY] = ' ';
    }
    else {
        grid[*cursorX][*cursorY] = '#';
    }
    if (direction == 0) {
        if (axis == 0) {
            --*cursorX;
        }
        else {
            --*cursorY;
        }
    }
    else {
        if (axis == 0) {
            ++*cursorX;
        }
        else {
            ++*cursorY;
        }
    }
    //wrap around cursor
    if (*cursorX >= gridWidth) {
        *cursorX = 0;
    }
    else if (*cursorX < 0) {
        *cursorX = gridWidth-1;
    }
    if (*cursorY >= gridHeight) {
        *cursorY = 0;
    }
    else if (*cursorY < 0) {
        *cursorY = gridHeight-1;
    }
    
    
    if (grid[*cursorX][*cursorY] == '#') {
        *marked = 1;
    }
    else {
        *marked = 0;
    }
}

int getInput(int gridWidth, int gridHeight, char grid[gridWidth][gridHeight]) {
    //get input
    //set up UI
    int cursorX = 0;
    int cursorY = 0;
    int finInput = 0;
    int marked = 0;
    char input;
    int population = 0;
    //allow user to interact with UI
    while (!finInput) {
        grid[cursorX][cursorY] = '0';
        if (update) {
            update = 0;
            puts("Input start conditions:");
            printGrid(gridWidth, gridHeight, grid);
            puts("a: left d:right w:up s:down enter:start game space:add/remove cell q:quit");
        }
        
        if (read(STDIN_FILENO, &input, 1) == 1) {
            if (input == 'd') {
                moveAxis(gridWidth, gridHeight, grid, &cursorX, &cursorY, 0, 1, &marked);
            }
            else if (input == 'a') {
                moveAxis(gridWidth, gridHeight, grid, &cursorX, &cursorY, 0, 0, &marked);
            }
            else if (input == 's') {
                moveAxis(gridWidth, gridHeight, grid, &cursorX, &cursorY, 1, 1, &marked);
            }
            else if (input == 'w') {
                moveAxis(gridWidth, gridHeight, grid, &cursorX, &cursorY, 1, 0, &marked);
            }
            else if (input == 'q') {
                exit(0);
            }
            else if (input == ' ') {
                update = 1;
                if (!marked) {
                    grid[cursorX][cursorY] = '#';
                    ++population;
                }
                else {
                    grid[cursorX][cursorY] = ' ';
                    --population;
                }
                ++cursorX;
                if (cursorX >= gridWidth) {
                    cursorX=0;
                }
                if (grid[cursorX][cursorY] == '#') {
                    marked = 1;
                }
                else {
                    marked = 0;
                }
            }
            else if (input == '\n') { //start generations once user has selected start conditions
                finInput = 1;
                if (!marked) {
                    grid[cursorX][cursorY] = ' ';
                }
                else {
                    grid[cursorX][cursorY] = '#';
                }
            }
        }
    }
    return population;
}

void *render(void *listInput) {
    struct List *list = (struct List *)listInput; 
    int gridWidth = 0;
    int gridHeight = 0;
    int centerX = 0;
    int centerY = 0;
    int quit = 0;
    char input;
    int paused = 0;
    char **grid;
    
    while (!quit) {
        //check width and height
        ioctl(0, TIOCGWINSZ, &win);
        int newGridWidth = win.ws_col;
        int newGridHeight = win.ws_row-3;
        if (newGridWidth != gridWidth || newGridHeight != gridHeight) {
            update = 1;
            gridWidth = newGridWidth;
            gridHeight = newGridHeight;
            grid = (char **)malloc(gridWidth * sizeof(char *));
            for (int i = 0; i < gridWidth; ++i) {
                grid[i] = (char *)malloc(gridHeight * sizeof(char));
            }
        }
        
        //detect input
        if (read(STDIN_FILENO, &input, 1) == 1) {
            pthread_mutex_lock(&mutex);
            if (input == 'd') {
                ++centerX;
                update = 1;
            }
            else if (input == 'a') {
                --centerX;
                update = 1;
            }
            else if (input == 's') {
                ++centerY;
                update = 1;
            }
            else if (input == 'w') {
                --centerY;
                update = 1;
            }
            else if (input == 'q') {
                //quit = 1;
                exit(0);
            }
            else if (input == '\n') {
                paused = 1;
                while (paused) {
                    if (read(STDIN_FILENO, &input, 1) == 1 && input == '\n') {
                        paused = 0;
                    }
                }
                //paused ^= 1;
            }
            pthread_mutex_unlock(&mutex);
        }
        
        if (update == 1) {
            update = 0;
            updateGrid(gridWidth, gridHeight, grid, centerX, centerY, list);
        }
    }
}

void updateList (struct List *list) {
    update = 1;
    int killAdjacents = 0;
    int birthAdjacents = 0;
    int numToAdd = 0;
    int numToRemove = 0;
    int found = 0;
    int duplicate = 0;
    int xCoord;
    int yCoord;
    int xCoord2;
    int yCoord2;
    //for every cell in list
        //for every adjacent coords
            //for every other cell in list (for so it won't find new cells)
                //compare cell coords with adjacent coords
                //if cell found:
                    //found = 1
                    //++killAdjacents
            //if found = 0:
                //for every adjacent coords
                    //for every other cell in list (for)
                        //compare coords
                        //if cell found:
                            //++birthAdjacents
                //if birthAdjacents == 3
                    //create new cell
                    //++numToAdd
        //if killAdjacents < 2 or > 3
            //mark cell to be deleted
            //++numToRemove
    //increment over list, removing those marked for deletion
    //work out new population and list size
    
    pthread_mutex_lock(&mutex);
    cell *currentCell = list->first;
    cell *currentCell2 = list->first;
    cell *lastCell;
    while (currentCell->next != NULL) {
        currentCell = currentCell->next;
    }
    lastCell = currentCell;
    
    currentCell = list->first;
    for (int cellNum = 0; cellNum < list->size; ++cellNum) {
        killAdjacents = 0;
        
        for (int vert = -1; vert < 2; ++vert) {
            for (int horiz = -1; horiz < 2; ++horiz) {
                found = 0;
                xCoord = currentCell->x + horiz;
                yCoord = currentCell->y + vert;
                currentCell2 = list->first;
                for (int cellNum2 = 0; cellNum2 < list->size; ++cellNum2) {
                    if (currentCell2->x == xCoord && currentCell2->y == yCoord) {
                        found = 1;
                        ++killAdjacents;
                    }
                    currentCell2 = currentCell2->next;
                }
                
                if (!found) {
                    birthAdjacents = 0;
                    
                    for (int vert2 = -1; vert2 < 2; ++vert2) {
                        for (int horiz2 = -1; horiz2 < 2; ++horiz2) {
                            xCoord2 = xCoord + horiz2;
                            yCoord2 = yCoord + vert2;
                            currentCell2 = list->first;
                            for (int cellNum2 = 0; cellNum2 < list->size; ++cellNum2) {
                                if (currentCell2->x == xCoord2 && currentCell2->y == yCoord2) {
                                    ++birthAdjacents;
                                }
                                currentCell2 = currentCell2->next;
                            }
                        }
                    }
                    
                    if (birthAdjacents == 3) {
                        //puts("make cell");
                        //printf("X: %d Y: %d \n", xCoord, yCoord);
                        //check for duplicate birth
                        duplicate = 0;
                        currentCell2 = list->first;
                        while (currentCell2 != NULL) {
                            if (currentCell2->x == xCoord && currentCell2->y == yCoord) {
                                duplicate = 1;
                            }
                            currentCell2 = currentCell2->next;
                        }
                        
                        if (!duplicate) {
                            cell *newCell = malloc(sizeof(cell));
                            newCell->x = xCoord;
                            newCell->y = yCoord;
                            newCell->toRemove = 0;
                            newCell->next = NULL;
                            newCell->last = lastCell;
                            lastCell->next = newCell;
                            lastCell = newCell;
                            ++numToAdd;
                        }
                    }
                }
            }
        }
        
        if (killAdjacents < 3 || killAdjacents > 4) {
            currentCell->toRemove = 1;
            ++numToRemove;
        }
        currentCell = currentCell->next;
        
    }
    
    //remove cells marked to delete
    cell *prevCell;
    cell *nextCell;
    currentCell = list->first;
    for (int cellNum = 0; cellNum < list->size; ++cellNum) {
        if (currentCell->toRemove == 1) {
            prevCell = currentCell->last;
            nextCell = currentCell->next;
            free(currentCell);
            //if last is NULL set new first, set next prev to NULL
            if (prevCell == NULL) {
                list->first = nextCell;
                nextCell->last = NULL;
            }
            else {
                prevCell->next = nextCell;
            }
            //if next is NULL set next of prev to NULL
            if (nextCell == NULL) {
                prevCell->next = NULL;
            }
            else {
                nextCell->last = prevCell;
            }
            
            currentCell = nextCell;
        }
        else {
            currentCell = currentCell->next;
        }
    }
    
    pop = pop + numToAdd - numToRemove;
    list->size = list->size + numToAdd - numToRemove;
    //printf("%d \n", list->size);
    
    ++gen;
    pthread_mutex_unlock(&mutex);
}

int main()
{
    changeTerminalSettings();

    //create grid / viewport
    //get width and height of terminal
    ioctl(0, TIOCGWINSZ, &win);
    int gridWidth = win.ws_col;
    int gridHeight = win.ws_row-3;
    //printf("Width: %d Height: %d \n", gridWidth, gridHeight);
    
    //init grid to all spaces
    char grid[gridWidth][gridHeight];
    for (int row = 0; row < gridHeight; ++row) {
        for (int col = 0; col < gridWidth; ++col) {
            grid[col][row] = ' ';
        }
    }
    
    //get input using UI
    pop = getInput(gridWidth, gridHeight, grid);
    update = 1;
    
    //convert to cell items in dynamic linked list
    //make list
    struct List list;
    list.size = 0;
    list.first = NULL;
    cell *previousCell = NULL;
    for (int col = 0; col < gridWidth; ++col) {
        for (int row = 0; row < gridHeight; ++row) {
            if (grid[col][row] == '#') {
                cell *currentCell = malloc(sizeof(cell));
                currentCell->x = col;
                currentCell->y = row;
                currentCell->toRemove = 0;
                currentCell->last = previousCell;
                currentCell->next = NULL;
                
                if (previousCell != NULL) {
                    previousCell->next = currentCell;
                }
                list.size++;
                if (list.first == NULL) {
                    list.first = currentCell;
                }
                previousCell = currentCell;
            }
        }
    }
    
    //Split into 2 threads
    pthread_t renderThread;
    pthread_create(&renderThread, NULL, render, &list);
    
    while (1) {
        sleep(1);     
        updateList(&list);
    }

    return 0;
}

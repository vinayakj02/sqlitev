#include<iostream>
#include<string>
#include<vector>
#include<sstream>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

#define process_normal_COMMANDS_SUCCESS 0
#define process_normal_INP_COMMAND_FAILURE 1
#define process_normal_SEL_COMMAND_FAILURE 2
#define TABLE_FILLED 3

// using a predifined table struct rn 
typedef struct{
    int id;
    string username;
    string email;
} Row;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute); // returns size of the attribute
const int ID_SIZE = size_of_attribute(Row, id)
const int USERNAME_SIZE = size_of_attribute(Row, username);
const int EMAIL_SIZE = size_of_attribute(Row, email);
const int ID_OFFSET = 0;
const int USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const int EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const int ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

const int PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100
const int ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const int TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;



// an abstraction over table, to read and write from a persistent file
typedef struct{
    int file_descriptior;
    int file_length;
    uint8_t* pages[TABLE_MAX_PAGES];
} Pager;

// Table is the way our data stored in the memory
typedef struct{
    int num_rows;                    // the number of rows currently in table
    // uint8_t* pages[TABLE_MAX_PAGES]; // an array of pointers to pages ( max 100 pages )
    Pager* pager;
} Table;



bool DEBUG = false;

void print_message(string message){
    if(DEBUG) printf("%s\n", &message);
}

vector<string> read_input(){
    cout << "sqlite > ";
    vector<string> input;
    string inp_line;
        
    getline(cin, inp_line);
    istringstream iss(inp_line);

    string token;
    while(iss >> token){
        input.push_back(token);
    }
    return input;
}

// commands which begin with . are meta commands, processed in this
void process_META_COMMANDS(vector<string> input){
    string string_exit(".exit");
    if(input[0].compare(string_exit) == 0){
        cout << "byeee\n";
        exit(EXIT_SUCCESS);
    }
    else{
        cout << "Unrecognized command\n";
    }
    
}

/*
scenario 

total num rows -> 50
rows per page -> ROWS_PER_PAGE -> 4
page num = int(50 / 4) -> 13th page

12th page
row 45
row 46
row 47
row 48

13th page (12th accd to indexing)
row 49
row 50

*/

uint8_t* get_page(Pager* pager, int page_number){
    if(page_number > TABLE_MAX_PAGES){
        printf("Exceeded page num, out of bounds");
        exit(EXIT_FAILURE);
    }

    // if it's NULL, it's a cache miss, meaning this row was never read or written in this session.
    if(pager->pages[page_number] == NULL){
        // allocate memory
        uint8_t* page = (uint8_t*)(PAGE_SIZE);
        int num_pages = pager->file_length/PAGE_SIZE;

        // if there's a partial page
        if(pager->file_length % PAGE_SIZE){
            num_pages++;
        }

        // we read the page
        if(page_number <= num_pages){
            //movess fd to fd + page_number * PAGE_SIZE
            lseek(pager->file_descriptior, page_number * PAGE_SIZE, SEEK_SET);
            //copy from fd to page
            ssize_t bytes_read = read(pager->file_descriptior, page, PAGE_SIZE);
            if(bytes_read == -1){
                printf("Error in reading file");
                exit(EXIT_FAILURE);
            }
        }

        pager->pages[page_number] = page;

    }

    return pager->pages[page_number];
    

}

// to get address of the row
uint8_t* row_slot_in_memory(Table* table, int row_number){
    print_message("row_slot_in_memory()");
    // theres no ( + 1 ) as its indexed from 0 in table->pages[]
    int page_number = int(row_number/ROWS_PER_PAGE);

    // uint8_t* page = table->pager->pages[page_number];
    // if(page == NULL){
    //     // this is when youre adding a new row and its in a new page 
    //     // allocate mem when trying to access page 
    //     page = static_cast<uint8_t*>(malloc(PAGE_SIZE));
    //     table->pages[page_number] = page;
    // }

    // get address of the page
    uint8_t* page = get_page(table->pager, page_number);

    // two rows inside 12th
    int row_offset = row_number % ROWS_PER_PAGE;

    // destination in memory is page mem + row_offset*ROW_SIZE
    int byte_offset = row_offset * ROW_SIZE;

    return page + byte_offset;
    
}


// copies data from row object into destination memory 
void serialize_row(Row* source, uint8_t* destination) {
    if (source == NULL) {
        print_message("Error: Row* source pointer passed to serialize_row() is NULL");
        return;
    }

    if (destination == NULL) {
        print_message("Error: uint8_t* destination pointer passed to serialize_row() is NULL");
        return;
    }

    print_message("serialize_row() called");
    //fprintf(stderr, "Source: %p, Destination: %p", (void*)source, (void*)destination);

    // Verify memory layout
    //fprintf(stderr, "Offsets: ID=%d, Username=%d, Email=%d\n", ID_OFFSET, USERNAME_OFFSET, EMAIL_OFFSET);

    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}


// copies data from source memory to destination row
void deserialize_row(uint8_t *source, Row* destination){

    print_message("deserialize_row() called");
    
    if (source == NULL) {
        print_message("Error: Row* source pointer passed to serialize_row() is NULL");
        return;
    }

    if (destination == NULL) {
        print_message("Error: uint8_t* destination pointer passed to serialize_row() is NULL");
        return;
    }
   
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE );
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE );
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE );
}


void print_row(Row* row){
    printf("(%d, %s, %s)", row->id, &row->username, &row->email);
}

// commands other than meta commands
int process_normal_COMMANDS(vector<string> input, Table* table){
    print_message("Enter normal_COMMANDS");


    string string_insert("insert");
    string string_select("select");

    if(input[0].compare(string_insert) == 0){
        // this is an insert statement
        print_message("Insert statement detected");

        if(input.size() < 4) return process_normal_INP_COMMAND_FAILURE;

        if(table->num_rows >= TABLE_MAX_ROWS){
            print_message("Table already filled, exiting");
            return TABLE_FILLED;
        }

        print_message("Reading row from input");
        Row row;
        row.id = stoi(input[1]);
        row.username = input[2]; 
        row.email = input[3];

        print_message("Row read from input completely.");

        serialize_row(&row, row_slot_in_memory(table, table->num_rows));
        table->num_rows++;

        print_message("Returning from normal_COMMANDS");
        return process_normal_COMMANDS_SUCCESS;
    }
    else if(input[0].compare(string_select) == 0){
        // this is a select statement
        // rn it just does select * from table;
        print_message("Select statement");

        Row row;
        for(int i=0;i<table->num_rows;i++){
            deserialize_row(row_slot_in_memory(table, i), &row);
            printf("\n");
            print_row(&row);
            
        }

        printf("\n\n%d rows returned\n",table->num_rows);

        print_message("Returning from normal_COMMANDS");
        return process_normal_COMMANDS_SUCCESS;
        
    }
    else{
        printf("Unrecognized keyword at the beginning\n");
    }
    
}

Pager* pager_open(string filename){
    int fd = open(filename.c_str(), O_RDWR | O_CREAT);
    if(fd == -1){
        printf("Unable to open the file\n");
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(fd, 0, SEEK_END);

    //Pager* pager = malloc(sizeof(Pager));
    Pager* pager = new Pager();
    pager->file_descriptior = fd;
    pager->file_length = file_length;
    
    // initially the pager abstrcation, pages are NULL, 
    // so nothing has been read, if read once then we load into this which is basically our memory or our cache
    for(int i=0;i<TABLE_MAX_PAGES;i++){
        pager->pages[i] = NULL;
    }

    return pager;

}

// Initializing a new table
Table* db_open(string filename){
    
    Pager* pager = pager_open(filename);
    int num_rows = pager->file_length / ROW_SIZE;
    Table* table = (Table*)malloc(sizeof(Table));
    table->num_rows = num_rows;
    table->pager = pager;

    return table;

}

void db_close(Table* table){
    
}

void process_input(vector<string> input, Table* table){
    print_message("process_input()");
    if(input.size() < 1){
        // empty command
        return;
    }
    if(input[0][0] == '.'){
        // meta command
        process_META_COMMANDS(input);
    }
    else{
        // normal command
        process_normal_COMMANDS(input, table);
    }
}

int main(){ 

    cout << "Welcome to sqlite\n";

    // creates a table object with 0 rows
    string filename = "sqlite.db";
    Table* table = db_open(filename);
    
    while(true){
        vector<string> input = read_input();
        process_input(input, table);

    }


    return 0;

}
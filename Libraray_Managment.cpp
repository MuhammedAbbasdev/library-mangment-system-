/*
 * ============================================================
 *         LIBRARY MANAGEMENT SYSTEM — C++ PROJECT
 *         Programming Fundamentals | FAST NUCES
 * ------------------------------------------------------------
 *  Group Members:
 *    CH Abdul Rehman  (25P-6545)
 *    Faizan Ahmed     (25P-6510)
 *    Muhammad Abbas   (25P-6517)
 * ============================================================
 *
 *  FEATURES IMPLEMENTED:
 *   [1] Book Management  — Add / Display / Search
 *   [2] Student Management — Add / Display
 *   [3] Issue Book       — Availability check + Date recording
 *   [4] Return Book      — Late days + Fine calculation
 *   [5] View Issued Books — Active issue records
 *   [6] File Handling    — Binary file persistence (.dat)
 *
 *  DATA FILES CREATED AT RUNTIME:
 *   books.dat | students.dat | records.dat
 * ============================================================
 */

#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <ctime>
using namespace std;

// ─────────────────────────────────────────────
//  CONSTANTS
// ─────────────────────────────────────────────
const int    MAX_RECORDS  = 200;
const float  FINE_PER_DAY = 10.0f;
const int    LOAN_DAYS    = 14;       // allowed borrowing period

const char*  BOOKS_FILE    = "books.dat";
const char*  STUDENTS_FILE = "students.dat";
const char*  RECORDS_FILE  = "records.dat";

// ─────────────────────────────────────────────
//  DATA STRUCTURES
// ─────────────────────────────────────────────

struct Date {
    int day;
    int month;
    int year;
};

struct Book {
    int  id;
    char title[100];
    char author[100];
    bool isIssued;
};

struct Student {
    int  id;
    char name[100];
};

struct IssueRecord {
    int  bookId;
    int  studentId;
    Date issueDate;
    bool active;     // true = book not yet returned
};

// ─────────────────────────────────────────────
//  UTILITY: DATE & TIME
// ─────────────────────────────────────────────

Date getCurrentDate() {
    time_t  t   = time(0);
    tm*     now = localtime(&t);
    Date d;
    d.day   = now->tm_mday;
    d.month = now->tm_mon + 1;
    d.year  = now->tm_year + 1900;
    return d;
}

long daysBetween(Date from, Date to) {
    tm t1 = {}, t2 = {};
    t1.tm_mday = from.day;   t1.tm_mon = from.month - 1;  t1.tm_year = from.year - 1900;
    t2.tm_mday = to.day;     t2.tm_mon = to.month - 1;    t2.tm_year = to.year - 1900;
    time_t time1 = mktime(&t1);
    time_t time2 = mktime(&t2);
    return (long)(difftime(time2, time1) / 86400.0);
}

void printDate(Date d) {
    cout << setfill('0') << setw(2) << d.day   << "/"
         << setfill('0') << setw(2) << d.month << "/"
         << d.year;
    cout << setfill(' ');   // reset fill
}

// ─────────────────────────────────────────────
//  UTILITY: UI HELPERS
// ─────────────────────────────────────────────

void printLine(int len = 55, char ch = '=') {
    cout << string(len, ch) << "\n";
}

void pauseScreen() {
    cout << "\n  Press Enter to continue...";
    cin.ignore();
    cin.get();
}

void clearScreen() {
    // Works on most terminals
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void printHeader() {
    clearScreen();
    printLine(55, '=');
    cout << "        LIBRARY MANAGEMENT SYSTEM\n";
    cout << "          FAST NUCES  |  PF Project\n";
    printLine(55, '=');
}

void printSuccess(const char* msg) {
    cout << "\n  [SUCCESS] " << msg << "\n";
}

void printError(const char* msg) {
    cout << "\n  [ERROR]   " << msg << "\n";
}

// ─────────────────────────────────────────────
//  FILE HELPERS: BOOKS
// ─────────────────────────────────────────────

// Returns true and fills 'b' if a book with given id exists
bool findBook(int bookId, Book& b) {
    ifstream fin(BOOKS_FILE, ios::binary);
    if (!fin) return false;
    while (fin.read((char*)&b, sizeof(Book))) {
        if (b.id == bookId) {
            fin.close();
            return true;
        }
    }
    fin.close();
    return false;
}

// Overwrites the matching book record in the file
void updateBookRecord(const Book& updated) {
    Book   all[MAX_RECORDS];
    int    count = 0;

    ifstream fin(BOOKS_FILE, ios::binary);
    while (fin.read((char*)&all[count], sizeof(Book))) count++;
    fin.close();

    for (int i = 0; i < count; i++) {
        if (all[i].id == updated.id) {
            all[i] = updated;
            break;
        }
    }

    ofstream fout(BOOKS_FILE, ios::binary | ios::trunc);
    for (int i = 0; i < count; i++)
        fout.write((char*)&all[i], sizeof(Book));
    fout.close();
}

// Returns true if book id already in file
bool bookIdExists(int id) {
    Book b;
    return findBook(id, b);
}

// ─────────────────────────────────────────────
//  FILE HELPERS: STUDENTS
// ─────────────────────────────────────────────

bool studentExists(int studentId) {
    Student    s;
    ifstream   fin(STUDENTS_FILE, ios::binary);
    if (!fin) return false;
    while (fin.read((char*)&s, sizeof(Student))) {
        if (s.id == studentId) {
            fin.close();
            return true;
        }
    }
    fin.close();
    return false;
}

bool studentIdExists(int id) {
    return studentExists(id);
}

// ─────────────────────────────────────────────
//  FILE HELPERS: ISSUE RECORDS
// ─────────────────────────────────────────────

// Returns true if a student already has an active issue
bool studentHasActiveIssue(int studentId) {
    IssueRecord rec;
    ifstream    fin(RECORDS_FILE, ios::binary);
    if (!fin) return false;
    while (fin.read((char*)&rec, sizeof(IssueRecord))) {
        if (rec.studentId == studentId && rec.active) {
            fin.close();
            return true;
        }
    }
    fin.close();
    return false;
}

// Finds active issue record for a given bookId. Returns idx, -1 if not found
int findActiveRecord(int bookId, IssueRecord records[], int& count) {
    count = 0;
    int idx = -1;
    ifstream fin(RECORDS_FILE, ios::binary);
    if (!fin) return -1;
    while (fin.read((char*)&records[count], sizeof(IssueRecord))) {
        if (records[count].bookId == bookId && records[count].active)
            idx = count;
        count++;
    }
    fin.close();
    return idx;
}

void saveAllRecords(IssueRecord records[], int count) {
    ofstream fout(RECORDS_FILE, ios::binary | ios::trunc);
    for (int i = 0; i < count; i++)
        fout.write((char*)&records[i], sizeof(IssueRecord));
    fout.close();
}

// ─────────────────────────────────────────────
//  MODULE 1: BOOK MANAGEMENT
// ─────────────────────────────────────────────

void addBook() {
    printHeader();
    cout << "  ── ADD NEW BOOK ──\n\n";

    Book b;
    b.isIssued = false;

    cout << "  Enter Book ID   : "; cin >> b.id;
    if (bookIdExists(b.id)) {
        printError("Book ID already exists!");
        pauseScreen();
        return;
    }

    cin.ignore();
    cout << "  Enter Title     : "; cin.getline(b.title,  100);
    cout << "  Enter Author    : "; cin.getline(b.author, 100);

    ofstream fout(BOOKS_FILE, ios::binary | ios::app);
    fout.write((char*)&b, sizeof(Book));
    fout.close();

    printSuccess("Book added successfully!");
    pauseScreen();
}

void displayAllBooks() {
    printHeader();
    cout << "  ── ALL BOOKS IN LIBRARY ──\n\n";

    ifstream fin(BOOKS_FILE, ios::binary);
    if (!fin) {
        printError("No book records found. Add books first.");
        pauseScreen();
        return;
    }

    cout << "  " << left
         << setw(6)  << "ID"
         << setw(32) << "Title"
         << setw(22) << "Author"
         << setw(12) << "Status" << "\n";
    cout << "  " << string(72, '-') << "\n";

    Book b;
    int  count = 0;
    while (fin.read((char*)&b, sizeof(Book))) {
        count++;
        cout << "  " << left
             << setw(6)  << b.id
             << setw(32) << b.title
             << setw(22) << b.author
             << setw(12) << (b.isIssued ? "Issued" : "Available") << "\n";
    }
    fin.close();

    if (count == 0)
        cout << "  No books available.\n";
    else
        cout << "\n  Total Books: " << count << "\n";

    pauseScreen();
}

void searchBook() {
    printHeader();
    cout << "  ── SEARCH BOOK ──\n\n";

    int id;
    cout << "  Enter Book ID to search: "; cin >> id;

    Book b;
    if (findBook(id, b)) {
        cout << "\n  ┌─ Book Found ─────────────────────────\n";
        cout << "  │  ID     : " << b.id     << "\n";
        cout << "  │  Title  : " << b.title  << "\n";
        cout << "  │  Author : " << b.author << "\n";
        cout << "  │  Status : " << (b.isIssued ? "Currently Issued" : "Available") << "\n";
        cout << "  └──────────────────────────────────────\n";
    } else {
        printError("Book not found!");
    }
    pauseScreen();
}

// ─────────────────────────────────────────────
//  MODULE 2: STUDENT MANAGEMENT
// ─────────────────────────────────────────────

void addStudent() {
    printHeader();
    cout << "  ── ADD NEW STUDENT ──\n\n";

    Student s;
    cout << "  Enter Student ID  : "; cin >> s.id;
    if (studentIdExists(s.id)) {
        printError("Student ID already exists!");
        pauseScreen();
        return;
    }

    cin.ignore();
    cout << "  Enter Student Name: "; cin.getline(s.name, 100);

    ofstream fout(STUDENTS_FILE, ios::binary | ios::app);
    fout.write((char*)&s, sizeof(Student));
    fout.close();

    printSuccess("Student registered successfully!");
    pauseScreen();
}

void displayAllStudents() {
    printHeader();
    cout << "  ── REGISTERED STUDENTS ──\n\n";

    ifstream fin(STUDENTS_FILE, ios::binary);
    if (!fin) {
        printError("No student records found. Add students first.");
        pauseScreen();
        return;
    }

    cout << "  " << left << setw(10) << "ID" << setw(30) << "Name" << "\n";
    cout << "  " << string(40, '-') << "\n";

    Student s;
    int     count = 0;
    while (fin.read((char*)&s, sizeof(Student))) {
        count++;
        cout << "  " << left << setw(10) << s.id << setw(30) << s.name << "\n";
    }
    fin.close();

    if (count == 0)
        cout << "  No students registered.\n";
    else
        cout << "\n  Total Students: " << count << "\n";

    pauseScreen();
}

// ─────────────────────────────────────────────
//  MODULE 3: ISSUE BOOK
// ─────────────────────────────────────────────

void issueBook() {
    printHeader();
    cout << "  ── ISSUE BOOK ──\n\n";

    int studentId, bookId;
    cout << "  Enter Student ID : "; cin >> studentId;
    cout << "  Enter Book ID    : "; cin >> bookId;

    // Validations
    if (!studentExists(studentId)) {
        printError("Student not found! Register the student first.");
        pauseScreen();
        return;
    }
    if (studentHasActiveIssue(studentId)) {
        printError("Student already has an issued book. Return it first.");
        pauseScreen();
        return;
    }

    Book b;
    if (!findBook(bookId, b)) {
        printError("Book not found! Add the book first.");
        pauseScreen();
        return;
    }
    if (b.isIssued) {
        printError("Book is already issued to another student.");
        pauseScreen();
        return;
    }

    // Create issue record
    IssueRecord r;
    r.bookId    = bookId;
    r.studentId = studentId;
    r.issueDate = getCurrentDate();
    r.active    = true;

    ofstream fout(RECORDS_FILE, ios::binary | ios::app);
    fout.write((char*)&r, sizeof(IssueRecord));
    fout.close();

    // Mark book as issued
    b.isIssued = true;
    updateBookRecord(b);

    cout << "\n  ┌─ Issue Confirmation ─────────────────\n";
    cout << "  │  Book    : " << b.title     << "\n";
    cout << "  │  Student : " << studentId   << "\n";
    cout << "  │  Date    : "; printDate(r.issueDate); cout << "\n";
    cout << "  │  Due By  : " << LOAN_DAYS << " days from issue date\n";
    cout << "  └──────────────────────────────────────\n";
    printSuccess("Book issued successfully!");
    pauseScreen();
}

// ─────────────────────────────────────────────
//  MODULE 4: RETURN BOOK + FINE CALCULATION
// ─────────────────────────────────────────────

void returnBook() {
    printHeader();
    cout << "  ── RETURN BOOK ──\n\n";

    int bookId;
    cout << "  Enter Book ID : "; cin >> bookId;

    IssueRecord records[MAX_RECORDS];
    int         totalRecords = 0;
    int         idx = findActiveRecord(bookId, records, totalRecords);

    if (idx == -1) {
        printError("No active issue record found for this book!");
        pauseScreen();
        return;
    }

    Date today    = getCurrentDate();
    long held     = daysBetween(records[idx].issueDate, today);
    long lateDays = held - LOAN_DAYS;

    // ── Return Summary ──────────────────────────────
    Book b;
    findBook(bookId, b);

    cout << "\n  ┌─ Return Summary ─────────────────────\n";
    cout << "  │  Book      : " << b.title       << "\n";
    cout << "  │  Student   : " << records[idx].studentId << "\n";
    cout << "  │  Issued on : "; printDate(records[idx].issueDate); cout << "\n";
    cout << "  │  Today     : "; printDate(today);                  cout << "\n";
    cout << "  │  Days Held : " << held << " day(s)\n";

    if (lateDays > 0) {
        float fine = (float)lateDays * FINE_PER_DAY;
        cout << "  │  Days Late : " << lateDays << " day(s)\n";
        cout << "  │  Fine      : Rs. " << fixed << setprecision(0) << fine << "\n";
    } else {
        cout << "  │  Status    : Returned on time! No fine.\n";
    }
    cout << "  └──────────────────────────────────────\n";

    // Update record
    records[idx].active = false;
    saveAllRecords(records, totalRecords);

    // Mark book as available
    b.isIssued = false;
    updateBookRecord(b);

    printSuccess("Book returned successfully!");
    pauseScreen();
}

// ─────────────────────────────────────────────
//  MODULE 5: VIEW CURRENTLY ISSUED BOOKS
// ─────────────────────────────────────────────

void viewIssuedBooks() {
    printHeader();
    cout << "  ── CURRENTLY ISSUED BOOKS ──\n\n";

    ifstream fin(RECORDS_FILE, ios::binary);
    if (!fin) {
        printError("No issue records found.");
        pauseScreen();
        return;
    }

    cout << "  " << left
         << setw(10) << "Book ID"
         << setw(12) << "Student ID"
         << setw(16) << "Issued On"
         << setw(10) << "Days Held"
         << setw(10) << "Status" << "\n";
    cout << "  " << string(62, '-') << "\n";

    IssueRecord rec;
    int         count   = 0;
    Date        today   = getCurrentDate();

    while (fin.read((char*)&rec, sizeof(IssueRecord))) {
        if (rec.active) {
            count++;
            long held     = daysBetween(rec.issueDate, today);
            long lateDays = held - LOAN_DAYS;

            cout << "  " << left
                 << setw(10) << rec.bookId
                 << setw(12) << rec.studentId;

            // Issue date
            cout << setfill('0')
                 << setw(2) << rec.issueDate.day   << "/"
                 << setw(2) << rec.issueDate.month << "/"
                 << rec.issueDate.year << "  ";
            cout << setfill(' ');

            cout << setw(10) << held;
            cout << (lateDays > 0 ? "OVERDUE" : "On Time") << "\n";
        }
    }
    fin.close();

    if (count == 0)
        cout << "  No books currently issued.\n";
    else
        cout << "\n  Active Issues: " << count << "\n";

    pauseScreen();
}

// ─────────────────────────────────────────────
//  MAIN MENU
// ─────────────────────────────────────────────

void displayMenu() {
    printHeader();
    cout << "\n";
    cout << "   BOOK MANAGEMENT\n";
    cout << "   [1] Add Book\n";
    cout << "   [2] Display All Books\n";
    cout << "   [3] Search Book\n";
    cout << "\n";
    cout << "   STUDENT MANAGEMENT\n";
    cout << "   [4] Add Student\n";
    cout << "   [5] Display All Students\n";
    cout << "\n";
    cout << "   TRANSACTIONS\n";
    cout << "   [6] Issue Book\n";
    cout << "   [7] Return Book\n";
    cout << "   [8] View Issued Books\n";
    cout << "\n";
    printLine(55, '-');
    cout << "   [0] Exit\n";
    printLine(55, '=');
    cout << "   Enter your choice: ";
}

// ─────────────────────────────────────────────
//  ENTRY POINT
// ─────────────────────────────────────────────

int main() {
    int choice;

    do {
        displayMenu();
        cin >> choice;

        switch (choice) {

            // ── Book Management
            case 1: addBook();           break;
            case 2: displayAllBooks();   break;
            case 3: searchBook();        break;

            // ── Student Management
            case 4: addStudent();        break;
            case 5: displayAllStudents();break;

            // ── Transactions
            case 6: issueBook();         break;
            case 7: returnBook();        break;
            case 8: viewIssuedBooks();   break;

            // ── Exit
            case 0:
                clearScreen();
                printLine(55, '=');
                cout << "   Thank you for using the Library System!\n";
                cout << "          FAST NUCES | Group Project\n";
                printLine(55, '=');
                break;

            default:
                printError("Invalid choice! Please enter a valid option.");
                pauseScreen();
        }

    } while (choice != 0);

    return 0;
}

/*
 * ============================================================
 *  HOW TO COMPILE AND RUN:
 *
 *   Using g++ (terminal):
 *     g++ LibraryManagementSystem.cpp -o Library
 *     ./Library           (Linux / macOS)
 *     Library.exe         (Windows)
 *
 *   Using Code::Blocks / Dev-C++:
 *     Create a new project → Console Application
 *     Paste this file → Build & Run (F9)
 *
 *  NOTES:
 *   - Three .dat files are auto-created on first run
 *     (books.dat, students.dat, records.dat)
 *   - Data persists between sessions via binary file I/O
 *   - Default loan period = 14 days | Fine = Rs. 10/day
 * ============================================================
 */

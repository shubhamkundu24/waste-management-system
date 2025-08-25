// Waste Management System
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <mysql/mysql.h>
#include <iomanip>
#include<bits/stdc++.h>

using namespace std;

const char* DB_HOST = "localhost";
const char* DB_USER = "root";
const char* DB_PASS = "shubhamlinux";
const char* DB_NAME = "waste_db";
const unsigned int DB_PORT = 3306;

class Utils {
public:
    static string getCurrentDateTime() {
        time_t now = time(0);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return string(buffer);
    }

    static string escapeString(MYSQL* conn, const string& input) {
        if (!conn) return input;
        
        char* escaped = new char[input.length() * 2 + 1];
        mysql_real_escape_string(conn, escaped, input.c_str(), input.length());
        string result(escaped);
        delete[] escaped;
        return result;
    }

    static int getInteger(const string& prompt, int min, int max) {
        int value;
        while (true) {
            cout << prompt;
            cin >> value;
            if (cin.fail() || value < min || value > max) {
                cout << "Please enter a valid number between " << min << " and " << max << endl;
                cin.clear();
                cin.ignore(1000, '\n');
            } else {
                cin.ignore();
                return value;
            }
        }
    }

    static double getDouble(const string& prompt, double min, double max) {
        double value;
        while (true) {
            cout << prompt;
            cin >> value;
            if (cin.fail() || value < min || value > max) {
                cout << "Please enter a valid number between " << min << " and " << max << endl;
                cin.clear();
                cin.ignore(1000, '\n');
            } else {
                cin.ignore();
                return value;
            }
        }
    }

    static void clearScreen() {
        cout << "\033[2J\033[1;1H"; 
    }
};

class DatabaseSetup {
public:
    static bool initializeDatabase() {
        MYSQL* conn = mysql_init(NULL);
        if (!conn) {
            cerr << "Failed to initialize MySQL" << endl;
            return false;
        }

        if (!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, NULL, DB_PORT, NULL, 0)) {
            cerr << "Database connection failed: " << mysql_error(conn) << endl;
            mysql_close(conn);
            return false;
        }

        string query = "CREATE DATABASE IF NOT EXISTS " + string(DB_NAME);
        if (mysql_query(conn, query.c_str())) {
            cerr << "Failed to create database: " << mysql_error(conn) << endl;
            mysql_close(conn);
            return false;
        }

        if (mysql_select_db(conn, DB_NAME)) {
            cerr << "Failed to select database: " << mysql_error(conn) << endl;
            mysql_close(conn);
            return false;
        }

        vector<string> tables = {
            "CREATE TABLE IF NOT EXISTS users ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "username VARCHAR(50) UNIQUE, "
            "password VARCHAR(50), "
            "role ENUM('ADMIN','OPERATOR'))",

            "CREATE TABLE IF NOT EXISTS trucks ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "reg_no VARCHAR(50) UNIQUE, "
            "capacity_kg INT, "
            "active BOOLEAN DEFAULT TRUE)",

            "CREATE TABLE IF NOT EXISTS routes ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "name VARCHAR(100), "
            "area VARCHAR(100))",

            "CREATE TABLE IF NOT EXISTS bins ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "code VARCHAR(50) UNIQUE, "
            "location VARCHAR(255), "
            "route_id INT NULL, "
            "capacity_l INT DEFAULT 0, "
            "fill_level TINYINT DEFAULT 0, "
            "last_cleared DATETIME NULL, "
            "FOREIGN KEY(route_id) REFERENCES routes(id) ON DELETE SET NULL)",

            "CREATE TABLE IF NOT EXISTS assignments ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "truck_id INT, "
            "route_id INT, "
            "assigned_at DATETIME, "
            "status ENUM('PENDING','COMPLETED') DEFAULT 'PENDING', "
            "FOREIGN KEY(truck_id) REFERENCES trucks(id), "
            "FOREIGN KEY(route_id) REFERENCES routes(id))",

            "CREATE TABLE IF NOT EXISTS pickups ("
            "id BIGINT AUTO_INCREMENT PRIMARY KEY, "
            "bin_id INT, "
            "truck_id INT, "
            "collected_at DATETIME, "
            "weight_kg DECIMAL(10,2), "
            "FOREIGN KEY(bin_id) REFERENCES bins(id), "
            "FOREIGN KEY(truck_id) REFERENCES trucks(id))",

            "CREATE TABLE IF NOT EXISTS recycling ("
            "id BIGINT AUTO_INCREMENT PRIMARY KEY, "
            "pickup_id BIGINT NULL, "
            "material ENUM('PLASTIC','PAPER','METAL','GLASS','ORGANIC','OTHER'), "
            "weight_kg DECIMAL(10,2), "
            "recycled_at DATETIME, "
            "FOREIGN KEY(pickup_id) REFERENCES pickups(id) ON DELETE SET NULL)"
        };

        for (const auto& table : tables) {
            if (mysql_query(conn, table.c_str())) {
                cerr << "Failed to create table: " << mysql_error(conn) << endl;
                mysql_close(conn);
                return false;
            }
        }

        string addUsers = "INSERT IGNORE INTO users (username, password, role) VALUES "
                          "('admin', 'admin123', 'ADMIN'), "
                          "('operator', 'operator123', 'OPERATOR')";
        mysql_query(conn, addUsers.c_str());

        mysql_close(conn);
        return true;
    }
};

class Database {
private:
    MYSQL* connection;
    static Database* instance;

    Database() {
        connection = mysql_init(NULL);
        if (!connection) {
            cerr << "MySQL initialization failed" << endl;
            return;
        }

        if (!mysql_real_connect(connection, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, NULL, 0)) {
            cerr << "Connection failed: " << mysql_error(connection) << endl;
            connection = NULL;
        }
    }

public:
    static Database& getInstance() {
        if (!instance) {
            instance = new Database();
        }
        return *instance;
    }

    MYSQL* getConnection() {
        return connection;
    }

    bool isConnected() {
        return connection != NULL;
    }

    ~Database() {
        if (connection) {
            mysql_close(connection);
        }
    }
};

Database* Database::instance = NULL;

struct UserSession {
    int id;
    string username;
    string role;
};

class AuthSystem {
public:
    static bool login(UserSession& session) {
        Database& db = Database::getInstance();
        if (!db.isConnected()) {
            cout << "Database not connected" << endl;
            return false;
        }

        string username, password;
        cout << "Username: ";
        getline(cin, username);
        cout << "Password: ";
        getline(cin, password);

        string escapedUser = Utils::escapeString(db.getConnection(), username);
        string escapedPass = Utils::escapeString(db.getConnection(), password);

        string query = "SELECT id, role FROM users WHERE username='" + escapedUser + 
                       "' AND password='" + escapedPass + "'";
        
        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Login query failed" << endl;
            return false;
        }

        MYSQL_RES* result = mysql_store_result(db.getConnection());
        if (!result) {
            cout << "Invalid credentials" << endl;
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        if (!row) {
            cout << "Invalid credentials" << endl;
            mysql_free_result(result);
            return false;
        }

        session.id = atoi(row[0]);
        session.username = username;
        session.role = row[1];

        mysql_free_result(result);
        cout << "Logged in as " << username << " (" << session.role << ")" << endl;
        return true;
    }
};

class TruckManager {
public:
    static void addTruck() {
        Database& db = Database::getInstance();
        string regNo;
        int capacity;

        cout << "Enter truck registration number: ";
        getline(cin, regNo);
        capacity = Utils::getInteger("Enter truck capacity (kg): ", 1, 10000);

        string escapedReg = Utils::escapeString(db.getConnection(), regNo);
        string query = "INSERT INTO trucks (reg_no, capacity_kg) VALUES ('" + 
                       escapedReg + "', " + to_string(capacity) + ")";

        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Error adding truck: " << mysql_error(db.getConnection()) << endl;
        } else {
            cout << "Truck added successfully!" << endl;
        }
    }

    static void listTrucks() {
        Database& db = Database::getInstance();
        string query = "SELECT id, reg_no, capacity_kg, active FROM trucks ORDER BY id";
        
        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Error fetching trucks: " << mysql_error(db.getConnection()) << endl;
            return;
        }

        MYSQL_RES* result = mysql_store_result(db.getConnection());
        if (!result) {
            cout << "No trucks found" << endl;
            return;
        }

        cout << "\nID | Registration | Capacity | Status" << endl;
        cout << "------------------------------------" << endl;
        
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            cout << row[0] << " | " << setw(12) << row[1] << " | " 
                 << setw(8) << row[2] << " | " 
                 << (atoi(row[3]) ? "Active" : "Inactive") << endl;
        }
        
        mysql_free_result(result);
    }
};

class RouteManager {
public:
    static void addRoute() {
        Database& db = Database::getInstance();
        string name, area;

        cout << "Enter route name: ";
        getline(cin, name);
        cout << "Enter area: ";
        getline(cin, area);

        string escapedName = Utils::escapeString(db.getConnection(), name);
        string escapedArea = Utils::escapeString(db.getConnection(), area);
        
        string query = "INSERT INTO routes (name, area) VALUES ('" + 
                       escapedName + "', '" + escapedArea + "')";

        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Error adding route: " << mysql_error(db.getConnection()) << endl;
        } else {
            cout << "Route added successfully!" << endl;
        }
    }

    static void listRoutes() {
        Database& db = Database::getInstance();
        string query = "SELECT id, name, area FROM routes ORDER BY id";
        
        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Error fetching routes: " << mysql_error(db.getConnection()) << endl;
            return;
        }

        MYSQL_RES* result = mysql_store_result(db.getConnection());
        if (!result) {
            cout << "No routes found" << endl;
            return;
        }

        cout << "\nID | Name | Area" << endl;
        cout << "-----------------------------" << endl;
        
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            cout << row[0] << " | " << setw(15) << row[1] << " | " << row[2] << endl;
        }
        
        mysql_free_result(result);
    }
};

class BinManager {
public:
    static void addBin() {
        Database& db = Database::getInstance();
        string code, location;
        int capacity, routeId;

        cout << "Enter bin code: ";
        getline(cin, code);
        cout << "Enter location: ";
        getline(cin, location);
        capacity = Utils::getInteger("Enter capacity (liters): ", 1, 10000);
        
        cout << "Enter route ID (0 for none): ";
        cin >> routeId;
        cin.ignore();

        string escapedCode = Utils::escapeString(db.getConnection(), code);
        string escapedLocation = Utils::escapeString(db.getConnection(), location);
        
        string query = "INSERT INTO bins (code, location, route_id, capacity_l) VALUES ('" + 
                       escapedCode + "', '" + escapedLocation + "', ";
        
        if (routeId > 0) {
            query += to_string(routeId);
        } else {
            query += "NULL";
        }
        
        query += ", " + to_string(capacity) + ")";

        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Error adding bin: " << mysql_error(db.getConnection()) << endl;
        } else {
            cout << "Bin added successfully!" << endl;
        }
    }

    static void listBins() {
        Database& db = Database::getInstance();
        string query = "SELECT b.id, b.code, b.location, r.name, b.capacity_l, b.fill_level, b.last_cleared "
                       "FROM bins b LEFT JOIN routes r ON b.route_id = r.id ORDER BY b.id";
        
        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Error fetching bins: " << mysql_error(db.getConnection()) << endl;
            return;
        }

        MYSQL_RES* result = mysql_store_result(db.getConnection());
        if (!result) {
            cout << "No bins found" << endl;
            return;
        }

        cout << "\nID | Code | Location | Route | Capacity | Fill % | Last Cleared" << endl;
        cout << "----------------------------------------------------------------" << endl;
        
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            cout << setw(2) << row[0] << " | " << setw(6) << row[1] << " | " 
                 << setw(15) << (strlen(row[2]) > 15 ? string(row[2]).substr(0, 12) + "..." : row[2]) << " | "
                 << setw(10) << (row[3] ? row[3] : "None") << " | "
                 << setw(8) << row[4] << " | "
                 << setw(6) << row[5] << " | "
                 << (row[6] ? row[6] : "Never") << endl;
        }
        
        mysql_free_result(result);
    }
};

class PickupManager {
public:
    static void recordPickup() {
        Database& db = Database::getInstance();
        int binId, truckId;
        double weight;

        BinManager::listBins();
        binId = Utils::getInteger("Enter bin ID: ", 1, 1000);
        
        TruckManager::listTrucks();
        truckId = Utils::getInteger("Enter truck ID: ", 1, 100);
        
        weight = Utils::getDouble("Enter weight collected (kg): ", 0.1, 1000.0);
        
        string datetime = Utils::getCurrentDateTime();
        string query = "INSERT INTO pickups (bin_id, truck_id, collected_at, weight_kg) VALUES (" +
                       to_string(binId) + ", " + to_string(truckId) + ", '" + datetime + "', " +
                       to_string(weight) + ")";

        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Error recording pickup: " << mysql_error(db.getConnection()) << endl;
            return;
        }

        string updateBin = "UPDATE bins SET fill_level = 0, last_cleared = '" + datetime + 
                           "' WHERE id = " + to_string(binId);
        mysql_query(db.getConnection(), updateBin.c_str());

        cout << "Pickup recorded successfully!" << endl;

        cout << "Record recycling for this pickup? (y/n): ";
        string choice;
        getline(cin, choice);
        
        if (choice == "y" || choice == "Y") {
            recordRecycling(mysql_insert_id(db.getConnection()));
        }
    }

    static void recordRecycling(long pickupId) {
        Database& db = Database::getInstance();
        int materialChoice;
        double weight;

        cout << "\nSelect material type:" << endl;
        cout << "1. Plastic\n2. Paper\n3. Metal\n4. Glass\n5. Organic\n6. Other" << endl;
        materialChoice = Utils::getInteger("Enter choice (1-6): ", 1, 6);
        
        weight = Utils::getDouble("Enter recycled weight (kg): ", 0.1, 1000.0);
        
        string material;
        switch (materialChoice) {
            case 1: material = "PLASTIC"; break;
            case 2: material = "PAPER"; break;
            case 3: material = "METAL"; break;
            case 4: material = "GLASS"; break;
            case 5: material = "ORGANIC"; break;
            default: material = "OTHER"; break;
        }

        string datetime = Utils::getCurrentDateTime();
        string query = "INSERT INTO recycling (pickup_id, material, weight_kg, recycled_at) VALUES (" +
                       to_string(pickupId) + ", '" + material + "', " + to_string(weight) + 
                       ", '" + datetime + "')";

        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Error recording recycling: " << mysql_error(db.getConnection()) << endl;
        } else {
            cout << "Recycling recorded successfully!" << endl;
        }
    }
};

class MenuSystem {
public:
    static void showAdminMenu() {
        int choice;
        do {
            Utils::clearScreen();
            cout << "=== ADMIN MENU ===" << endl;
            cout << "1. Manage Trucks" << endl;
            cout << "2. Manage Routes" << endl;
            cout << "3. Manage Bins" << endl;
            cout << "4. Record Pickup" << endl;
            cout << "5. View Reports" << endl;
            cout << "0. Logout" << endl;
            
            choice = Utils::getInteger("Enter your choice: ", 0, 5);
            
            switch (choice) {
                case 1: showTruckMenu(); break;
                case 2: showRouteMenu(); break;
                case 3: showBinMenu(); break;
                case 4: PickupManager::recordPickup(); break;
                case 5: showReports(); break;
                case 0: cout << "Logging out..." << endl; break;
            }
            
            if (choice != 0) {
                cout << "\nPress Enter to continue...";
                cin.ignore();
            }
        } while (choice != 0);
    }

    static void showOperatorMenu() {
        int choice;
        do {
            Utils::clearScreen();
            cout << "=== OPERATOR MENU ===" << endl;
            cout << "1. Record Pickup" << endl;
            cout << "2. View Bins Status" << endl;
            cout << "0. Logout" << endl;
            
            choice = Utils::getInteger("Enter your choice: ", 0, 2);
            
            switch (choice) {
                case 1: PickupManager::recordPickup(); break;
                case 2: BinManager::listBins(); break;
                case 0: cout << "Logging out..." << endl; break;
            }
            
            if (choice != 0) {
                cout << "\nPress Enter to continue...";
                cin.ignore();
            }
        } while (choice != 0);
    }

private:
    static void showTruckMenu() {
        int choice;
        do {
            Utils::clearScreen();
            cout << "=== TRUCK MANAGEMENT ===" << endl;
            cout << "1. Add Truck" << endl;
            cout << "2. List Trucks" << endl;
            cout << "0. Back" << endl;
            
            choice = Utils::getInteger("Enter your choice: ", 0, 2);
            
            switch (choice) {
                case 1: TruckManager::addTruck(); break;
                case 2: TruckManager::listTrucks(); break;
            }
        } while (choice != 0);
    }

    static void showRouteMenu() {
        int choice;
        do {
            Utils::clearScreen();
            cout << "=== ROUTE MANAGEMENT ===" << endl;
            cout << "1. Add Route" << endl;
            cout << "2. List Routes" << endl;
            cout << "0. Back" << endl;
            
            choice = Utils::getInteger("Enter your choice: ", 0, 2);
            
            switch (choice) {
                case 1: RouteManager::addRoute(); break;
                case 2: RouteManager::listRoutes(); break;
            }
        } while (choice != 0);
    }

    static void showBinMenu() {
        int choice;
        do {
            Utils::clearScreen();
            cout << "=== BIN MANAGEMENT ===" << endl;
            cout << "1. Add Bin" << endl;
            cout << "2. List Bins" << endl;
            cout << "0. Back" << endl;
            
            choice = Utils::getInteger("Enter your choice: ", 0, 2);
            
            switch (choice) {
                case 1: BinManager::addBin(); break;
                case 2: BinManager::listBins(); break;
            }
        } while (choice != 0);
    }

    static void showReports() {
        Database& db = Database::getInstance();
        string today = Utils::getCurrentDateTime().substr(0, 10);
        
        cout << "=== DAILY REPORT ===" << endl;
        cout << "Date: " << today << endl;
        
        string query = "SELECT COUNT(*), SUM(weight_kg) FROM pickups WHERE DATE(collected_at) = '" + today + "'";
        if (mysql_query(db.getConnection(), query.c_str())) {
            cout << "Error generating report" << endl;
            return;
        }
        
        MYSQL_RES* result = mysql_store_result(db.getConnection());
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            cout << "Pickups today: " << (row[0] ? row[0] : "0") << endl;
            cout << "Total waste collected: " << (row[1] ? row[1] : "0") << " kg" << endl;
            mysql_free_result(result);
        }
        
        query = "SELECT material, SUM(weight_kg) FROM recycling WHERE DATE(recycled_at) = '" + today + "' GROUP BY material";
        if (!mysql_query(db.getConnection(), query.c_str())) {
            result = mysql_store_result(db.getConnection());
            if (result && mysql_num_rows(result) > 0) {
                cout << "\nRecycling by material:" << endl;
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(result))) {
                    cout << row[0] << ": " << row[1] << " kg" << endl;
                }
            }
            mysql_free_result(result);
        }
    }
};

int main() {
    cout << "=== Waste Management System ===" << endl;
    
    if (!DatabaseSetup::initializeDatabase()) {
        cout << "Failed to initialize database. Exiting." << endl;
        return 1;
    }
    
    Database& db = Database::getInstance();
    if (!db.isConnected()) {
        cout << "Failed to connect to database. Exiting." << endl;
        return 1;
    }
    
    cout << "Database connected successfully!" << endl;
    
    UserSession session;
    while (!AuthSystem::login(session)) {
        cout << "Login failed. Try again? (y/n): ";
        string choice;
        getline(cin, choice);
        if (choice != "y" && choice != "Y") {
            return 0;
        }
    }
    
    if (session.role == "ADMIN") {
        MenuSystem::showAdminMenu();
    } else {
        MenuSystem::showOperatorMenu();
    }
    
    cout << "Thank you for using Waste Management System!" << endl;
    return 0;
}



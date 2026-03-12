#include <iostream>
#include <string>
#include "db.h"

int main() {
    try {
        Database db("logs/db.log");

        std::cout << "=== Database Test ===\n";

        // Test insert
        std::cout << "Inserting key 'name' with value 'Alice'\n";
        db.insert("name", "Alice");

        std::cout << "Inserting key 'age' with value '25'\n";
        db.insert("age", "25");

        std::cout << "Inserting key 'city' with value 'New York'\n";
        db.insert("city", "New York");

        // Test get
        std::cout << "\nGetting values:\n";
        std::cout << "name: " << db.get("name") << "\n";
        std::cout << "age: " << db.get("age") << "\n";
        std::cout << "city: " << db.get("city") << "\n";
        std::cout << "nonexistent: '" << db.get("nonexistent") << "' (should be empty)\n";

        // Test update (insert same key)
        std::cout << "\nUpdating 'age' to '26'\n";
        db.insert("age", "26");
        std::cout << "age: " << db.get("age") << "\n";

        // Test delete
        std::cout << "\nDeleting 'city'\n";
        db.deleteKey("city");
        std::cout << "city: '" << db.get("city") << "' (should be empty)\n";

        // Test delete nonexistent
        std::cout << "\nDeleting nonexistent key 'job'\n";
        db.deleteKey("job");  // Should not crash

        std::cout << "\n=== Test Complete ===\n";
        std::cout << "Check db.log for the log entries\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

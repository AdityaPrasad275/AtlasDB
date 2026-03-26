#include <iostream>

#include "db.h"

int main() {
    try {
        Database db("logs/dev.db");

        db.insert("hello", "world");
        std::cout << "hello -> " << db.get("hello") << '\n';

        db.deleteKey("hello");
        std::cout << "hello after delete -> " << db.get("hello") << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Development run failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}

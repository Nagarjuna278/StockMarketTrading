# Real-Time Stock Trading Engine

This project implements a real-time stock trading engine designed to process and match buy and sell orders across 1,024 unique tickers. The system is built to handle concurrent order submissions from multiple brokers in a multithreaded environment, utilizing lock-free data structures to ensure thread safety without traditional locking mechanisms. Below is an overview of the main components, the requirements, and how they are addressed in the implementation.

---

## Main Components

### 1. `SimpleRandom`
- **Purpose**: A lightweight pseudo-random number generator based on a linear congruential algorithm.
- **Key Methods**:
  - `nextInt()`: Generates the next unsigned long integer in the sequence.
  - `randInt(int low, int high)`: Returns a random integer within the specified range.
  - `uniform(double low, double high)`: Generates a random double between the given bounds.
- **Usage**: Drives the simulation by providing random values for order quantities, prices, and ticker selections.

### 2. `TickerString`
- **Purpose**: A fixed-length string class to represent ticker symbols (up to 16 characters).
- **Key Features**:
  - Stores characters in a `char` array with a null terminator.
  - Provides constructors for empty strings and C-string initialization.
  - Offers `c_str()` for string access and `operator[]` for character indexing.
- **Usage**: Ensures efficient ticker symbol management without dynamic memory allocation.

### 3. `Order`
- **Purpose**: Represents a single stock order.
- **Attributes**:
  - `orderType`: Enum value (`BUY` or `SELL`).
  - `ticker`: The `TickerString` identifying the stock.
  - `quantity`: Number of shares (marked `volatile` for thread-safe updates).
  - `price`: Price per share.
  - `orderId`: A unique identifier generated randomly.
- **Usage**: Encapsulates order details for processing and matching.

### 4. `OrderNode` and `OrderList`
- **Purpose**: Implements a lock-free singly linked list to store orders.
- **Details**:
  - **`OrderNode`**:
    - Contains an `Order` and a `volatile` pointer to the next node.
    - Used as a building block for the list.
  - **`OrderList`**:
    - Maintains a `volatile` head pointer.
    - `append(const Order&)`: Adds a new node using compare-and-swap (`__sync_bool_compare_and_swap`) for thread-safe insertion.
    - `getHead()`: Retrieves the list head for traversal.
- **Usage**: Provides a thread-safe structure for managing buy and sell orders.

### 5. `OrderBook`
- **Purpose**: Manages buy and sell orders for a specific ticker and executes trades.
- **Key Features**:
  - Contains separate `OrderList` instances for buy (`buyOrders`) and sell (`sellOrders`) orders.
  - `addOrder(const Order&)`: Adds an order and attempts to match it with opposite orders.
  - `findBestOpposite(const Order&, const OrderList&)`: Identifies the best matching order (e.g., lowest sell price for a buy order).
- **Matching Logic**: Matches orders when a buy price is ≥ the lowest sell price, adjusting quantities atomically using compare-and-swap.
- **Usage**: Core component for order processing and trade execution per ticker.

### 6. Global `orderBooks`
- **Purpose**: A fixed-size array of 1,024 `OrderBook` instances, one per ticker.
- **Management**:
  - Initialized by `initOrderBooks()` and deallocated by `cleanupOrderBooks()`.
  - Orders are routed to the correct `OrderBook` using `getOrderBookIndex()`, which hashes ticker symbols to array indices.
- **Usage**: Provides a scalable way to handle multiple tickers without dynamic mappings.

### 7. Utility Functions
- `getOrderBookIndex(const TickerString&)`: Computes an array index from a ticker symbol using a simple hash function.
- `addOrder(OrderType, const TickerString&, int, double)`: Creates an `Order` and delegates it to the appropriate `OrderBook`.
- `generateTickerSymbol(int)`: Generates ticker names like "TICKER0", "TICKER1", etc.
- `initTickers()` and `cleanupTickers()`: Manage an array of pre-generated ticker symbols.

### 8. Simulation Components
- `simulateTransactions(int)`: Generates random orders for simulation.
- `brokerFunction(int, int)`: Simulates a broker by repeatedly calling `simulateTransactions`.
- `runSimulation()`: Orchestrates the simulation by initializing resources, spawning broker threads, and cleaning up.

---

## Requirements and Solutions

### Requirement 1: Implement `addOrder` Function
- **Specification**: Must accept Order Type (Buy/Sell), Ticker Symbol, Quantity, and Price.
- **Solution**:
  - Implemented as `void addOrder(OrderType, const TickerString&, int, double)`.
  - Creates an `Order` object and forwards it to the appropriate `OrderBook` via `OrderBook::addOrder`.

### Requirement 2: Support 1,024 Tickers
- **Solution**:
  - Defined `NUM_TICKERS = 1024`.
  - Used a static array `OrderBook* orderBooks = new OrderBook[NUM_TICKERS]` to store order books.
  - Ticker symbols are mapped to indices using a hash function in `getOrderBookIndex`.

### Requirement 3: Simulate Active Stock Transactions
- **Specification**: Create a wrapper to randomly execute `addOrder`.
- **Solution**:
  - `simulateTransactions` generates random orders with varying types, tickers, quantities, and prices.
  - `brokerFunction` invokes `simulateTransactions` multiple times per broker.
  - `runSimulation` spawns multiple threads to simulate concurrent broker activity.

### Requirement 4: Implement `matchOrder` Function
- **Specification**: Match Buy orders with Sell orders when Buy price ≥ lowest Sell price.
- **Solution**:
  - Integrated into `OrderBook::addOrder`.
  - `findBestOpposite` scans the opposite order list to find the best match.
  - Trades are executed by atomically reducing quantities of matched orders.

### Requirement 5: Handle Race Conditions in Multithreading
- **Specification**: Use lock-free data structures.
- **Solution**:
  - Used `volatile` for shared variables (e.g., `quantity`, `next`, `head`) to ensure visibility across threads.
  - Employed `__sync_bool_compare_and_swap` for atomic updates in `OrderList::append` and quantity adjustments during matching.
  - Avoided mutexes or locks, relying on compare-and-swap for concurrency control.

### Requirement 6: Avoid Dictionaries or Maps
- **Solution**:
  - Replaced dynamic mappings with a fixed-size `orderBooks` array.
  - Used a custom hash function in `getOrderBookIndex` to map tickers to indices.
  - Avoided STL containers like `std::map` or `std::unordered_map`.

### Requirement 7: O(n) Time Complexity for Matching
- **Specification**: `matchOrder` must run in O(n), where n is the number of orders.
- **Solution**:
  - `findBestOpposite` performs a linear scan of the opposite order list, achieving O(n) complexity.
  - No sorting or complex data structures are used, ensuring the requirement is met.

---

## Usage
To run the simulation:
1. **Compile the Code**: Use a C++ compiler supporting threads (e.g., `g++ -std=c++11 -pthread`).
2. **Execute the Program**: Call `runSimulation()`, which initializes the system, spawns 5 broker threads, and simulates 200 iterations of 5 transactions each.
3. **Observe Output**: Trade execution messages will be printed to the console.

---

## Conclusion
This stock trading engine meets all specified requirements, delivering a thread-safe, efficient, and scalable solution for real-time order matching. It uses lock-free techniques, custom data structures, and a straightforward hashing approach to manage 1,024 tickers, providing a robust foundation for simulating stock market activity.
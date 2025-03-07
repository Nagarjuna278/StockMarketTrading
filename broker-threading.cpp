#include <stdio.h>
#include <thread>

// Simple Linear Congruential Generator for pseudo-random numbers
class SimpleRandom {
private:
    unsigned long state;

public:
    SimpleRandom(unsigned long seed = 12345) : state(seed) {}

    unsigned long nextInt() {
        // Parameters for a simple LCG
        const unsigned long a = 1664525;
        const unsigned long c = 1013904223;
        state = (a * state + c) % 0xFFFFFFFF; // 2^32
        return state;
    }

    int randInt(int low, int high) {
        // Generate random integer between low and high (inclusive)
        int range = high - low + 1;
        return low + (nextInt() % range);
    }

    double uniform(double low, double high) {
        // Generate random float between low and high
        double randFloat = nextInt() / (double)0xFFFFFFFF;
        return low + (randFloat * (high - low));
    }
};

// Global random number generator
SimpleRandom rng;

// Total number of distinct tickers supported
const int NUM_TICKERS = 1024;

// Maximum length for ticker symbols
const int MAX_TICKER_LENGTH = 16;

// Simple fixed-length string class for tickers
class TickerString {
private:
    char data[MAX_TICKER_LENGTH];

public:
    TickerString() {
        data[0] = '\0';
    }

    TickerString(const char* str) {
        int i = 0;
        while (str[i] != '\0' && i < MAX_TICKER_LENGTH - 1) {
            data[i] = str[i];
            i++;
        }
        data[i] = '\0';
    }

    const char* c_str() const {
        return data;
    }

    char operator[](int index) const {
        return data[index];
    }
};

// Order types
enum OrderType {
    BUY, 
    SELL
};

// Order class to represent a buy or sell order
class Order {
public:
    OrderType orderType;
    TickerString ticker;
    int quantity;
    double price;
    int orderId;

    Order() : orderType(BUY), quantity(0), price(0.0), orderId(0) {}

    Order(OrderType type, const TickerString& tkr, int qty, double prc) 
        : orderType(type), ticker(tkr), quantity(qty), price(prc) {
        // Generate a random order id
        orderId = rng.randInt(1, 1000000);
    }
};

// Simple implementation of a dynamic array for orders
class OrderList {
private:
    static const int INITIAL_CAPACITY = 10;
    Order* orders;
    int capacity;
    int size;

    void resize(int newCapacity) {
        Order* newOrders = new Order[newCapacity];
        for (int i = 0; i < size; i++) {
            newOrders[i] = orders[i];
        }
        delete[] orders;
        orders = newOrders;
        capacity = newCapacity;
    }

public:
    OrderList() : capacity(INITIAL_CAPACITY), size(0) {
        orders = new Order[capacity];
    }

    ~OrderList() {
        delete[] orders;
    }

    // Copy constructor
    OrderList(const OrderList& other) : capacity(other.capacity), size(other.size) {
        orders = new Order[capacity];
        for (int i = 0; i < size; i++) {
            orders[i] = other.orders[i];
        }
    }

    void append(const Order& order) {
        if (size >= capacity) {
            resize(capacity * 2);
        }
        orders[size++] = order;
    }

    Order& operator[](int index) {
        return orders[index];
    }

    int getSize() const {
        return size;
    }

    void removeAt(int index) {
        if (index < 0 || index >= size) {
            return;
        }
        for (int i = index; i < size - 1; i++) {
            orders[i] = orders[i + 1];
        }
        size--;
    }

    bool isEmpty() const {
        return size == 0;
    }
};

// OrderBook class to manage buy and sell orders for a ticker
class OrderBook {
private:
    OrderList buyOrders;
    OrderList sellOrders;
    volatile bool inTransaction;  // Flag for transaction safety

public:
    OrderBook() : inTransaction(false) {}

    void addOrder(const Order& order) {
        // Simple spinlock for thread safety
        while (true) {
            if (!inTransaction) {
                inTransaction = true;
                break;
            }
            // Yield to other threads
            std::this_thread::yield();
        }
        
        // Now we have exclusive access
        if (order.orderType == BUY) {
            buyOrders.append(order);
        } else {
            sellOrders.append(order);
        }
        
        matchOrders();
        
        inTransaction = false; // Release the "lock"
    }

    // Match buy and sell orders with O(n) time complexity
    void matchOrders() {
        while (!buyOrders.isEmpty() && !sellOrders.isEmpty()) {
            // Find best buy order (highest price) - O(n)
            int bestBuyIndex = 0;
            double bestBuyPrice = buyOrders[0].price;
            for (int i = 1; i < buyOrders.getSize(); i++) {
                if (buyOrders[i].price > bestBuyPrice) {
                    bestBuyPrice = buyOrders[i].price;
                    bestBuyIndex = i;
                }
            }

            // Find best sell order (lowest price) - O(n)
            int bestSellIndex = 0;
            double bestSellPrice = sellOrders[0].price;
            for (int i = 1; i < sellOrders.getSize(); i++) {
                if (sellOrders[i].price < bestSellPrice) {
                    bestSellPrice = sellOrders[i].price;
                    bestSellIndex = i;
                }
            }

            // If no match is possible, exit
            if (bestBuyPrice < bestSellPrice) {
                break;
            }

            // Execute the trade
            Order& buyOrder = buyOrders[bestBuyIndex];
            Order& sellOrder = sellOrders[bestSellIndex];

            // Determine the trade quantity
            int tradeQty = (buyOrder.quantity < sellOrder.quantity) ? 
                buyOrder.quantity : sellOrder.quantity;
            double tradePrice = sellOrder.price;  // Trade at the sell price

            printf("Trade executed for ticker %s: %d shares at %.2f\n", 
                   buyOrder.ticker.c_str(), tradeQty, tradePrice);

            // Deduct the traded quantity from both orders
            buyOrder.quantity -= tradeQty;
            sellOrder.quantity -= tradeQty;

            // Remove orders that have been completely filled
            if (buyOrder.quantity <= 0) {
                buyOrders.removeAt(bestBuyIndex);
            }
            if (sellOrder.quantity <= 0) {
                sellOrders.removeAt(bestSellIndex);
            }
        }
    }
};

// Global fixed-size array of order books
OrderBook* orderBooks = NULL;

// Initialize the order books
void initOrderBooks() {
    orderBooks = new OrderBook[NUM_TICKERS];
}

// Clean up the order books
void cleanupOrderBooks() {
    delete[] orderBooks;
}

// Get the index of the order book for a ticker
int getOrderBookIndex(const TickerString& ticker) {
    // Simple hash function based on the sum of ASCII codes mod NUM_TICKERS
    int total = 0;
    for (int i = 0; ticker[i] != '\0'; i++) {
        total += (int)ticker[i];
    }
    return total % NUM_TICKERS;
}

/**
 * addOrder - Adds a new order to the trading system
 * 
 * @param orderType: BUY or SELL
 * @param ticker: Ticker symbol for the stock
 * @param quantity: Number of shares
 * @param price: Price per share
 */
void addOrder(OrderType orderType, const TickerString& ticker, int quantity, double price) {
    int idx = getOrderBookIndex(ticker);
    Order order(orderType, ticker, quantity, price);
    orderBooks[idx].addOrder(order);
}

// Function to generate a ticker symbol
TickerString generateTickerSymbol(int index) {
    char buffer[MAX_TICKER_LENGTH];
    snprintf(buffer, MAX_TICKER_LENGTH, "TICKER%d", index);
    return TickerString(buffer);
}


// Array of ticker symbols
TickerString* tickers = NULL;

// Initialize ticker symbols
void initTickers() {
    tickers = new TickerString[NUM_TICKERS];
    for (int i = 0; i < NUM_TICKERS; i++) {
        tickers[i] = generateTickerSymbol(i);
    }
}

// Clean up ticker symbols
void cleanupTickers() {
    delete[] tickers;
}

/**
 * simulateTransactions - Generates random orders to simulate market activity
 * 
 * @param numTransactions: Number of orders to generate
 */
void simulateTransactions(int numTransactions = 10) {
    for (int i = 0; i < numTransactions; i++) {
        // Randomly choose buy or sell
        OrderType orderType = (rng.randInt(0, 1) == 0) ? BUY : SELL;
        
        // Randomly choose a ticker
        int tickerIndex = rng.randInt(0, NUM_TICKERS - 1);
        TickerString ticker = tickers[tickerIndex];
        
        // Generate random quantity and price
        int quantity = rng.randInt(1, 100);
        double price = rng.uniform(10.0, 100.0);
        // Round to 2 decimal places
        price = (int)(price * 100) / 100.0;
        
        // Add the order
        addOrder(orderType, ticker, quantity, price);
    }
}

// Broker function to run in a separate thread
void brokerFunction(int brokerId, int iterations) {
    for (int i = 0; i < iterations; i++) {
        simulateTransactions(5);
        
        // Add a small delay between batches of transactions
        // Use std::this_thread::yield() to give other threads a chance
        for (int j = 0; j < 100; j++) {
            std::this_thread::yield();
        }
    }
    printf("Broker %d completed activities\n", brokerId);
}

// Main simulation function
void runSimulation() {
    printf("Starting stock exchange simulation with threads...\n");
    
    // Initialize order books and tickers
    initOrderBooks();
    initTickers();
    
    // Create threads for the brokers
    // Create threads for the brokers
    std::thread brokerThreads[5];
    for (int i = 0; i < 5; i++) {
        brokerThreads[i] = std::thread([i]() { 
            brokerFunction(i, 200); 
        });
    }

    
    // Wait for all broker threads to complete
    for (int i = 0; i < 5; i++) {
        brokerThreads[i].join();
    }
    
    printf("Simulation completed\n");
    
    // Cleanup
    cleanupTickers();
    cleanupOrderBooks();
}

// Main function
int main() {
    runSimulation();
    return 0;
}

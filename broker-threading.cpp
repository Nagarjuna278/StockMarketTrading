#include <stdio.h>
#include <thread>

// Simple random number generator class
class SimpleRandom {
private:
    unsigned long state;

public:
    SimpleRandom(unsigned long seed = 12345) : state(seed) {}

    unsigned long nextInt() {
        const unsigned long a = 1664525;
        const unsigned long c = 1013904223;
        state = (a * state + c) % 0xFFFFFFFF;
        return state;
    }

    int randInt(int low, int high) {
        int range = high - low + 1;
        return low + (nextInt() % range);
    }

    double uniform(double low, double high) {
        double randFloat = nextInt() / (double)0xFFFFFFFF;
        return low + (randFloat * (high - low));
    }
};

SimpleRandom rng;

// Constants and TickerString class
const int NUM_TICKERS = 1024;
const int MAX_TICKER_LENGTH = 16;
const double MAX_DOUBLE = 1e308;

class TickerString {
private:
    char data[MAX_TICKER_LENGTH];

public:
    TickerString() { data[0] = '\0'; }

    TickerString(const char* str) {
        int i = 0;
        while (str[i] != '\0' && i < MAX_TICKER_LENGTH - 1) {
            data[i] = str[i];
            i++;
        }
        data[i] = '\0';
    }

    const char* c_str() const { return data; }

    char operator[](int index) const { return data[index]; }
};

// Order types and Order class
enum OrderType { BUY, SELL };

class Order {
public:
    OrderType orderType;
    TickerString ticker;
    volatile int quantity;
    double price;
    int orderId;

    Order(OrderType type, const TickerString& tkr, int qty, double prc)
        : orderType(type), ticker(tkr), quantity(qty), price(prc) {
        orderId = rng.randInt(1, 1000000);
    }
};

// OrderNode and OrderList classes
struct OrderNode {
    Order order;
    volatile OrderNode* next;
    OrderNode(const Order& ord) : order(ord), next(nullptr) {}
};

class OrderList {
private:
    volatile OrderNode* head;

public:
    OrderList() : head(nullptr) {}

    OrderNode* append(const Order& order) {
        OrderNode* newNode = new OrderNode(order);
        OrderNode* oldHead;
        do {
            oldHead = const_cast<OrderNode*>(head);
            newNode->next = oldHead;
        } while (!__sync_bool_compare_and_swap(&head, oldHead, newNode));
        return newNode;
    }

    volatile OrderNode* getHead() const { return head; }
};

// OrderBook class with matching logic
class OrderBook {
private:
    OrderList buyOrders;
    OrderList sellOrders;

    Order* findBestOpposite(const Order& order, const OrderList& oppositeOrders);

public:
    void addOrder(const Order& newOrder) {
        OrderList& orders = (newOrder.orderType == BUY) ? buyOrders : sellOrders;
        OrderList& oppositeOrders = (newOrder.orderType == BUY) ? sellOrders : buyOrders;

        OrderNode* newNode = orders.append(newOrder);

        while (newNode->order.quantity > 0) {
            Order* bestOpposite = findBestOpposite(newNode->order, oppositeOrders);
            if (!bestOpposite || bestOpposite->quantity <= 0) {
                break;
            }

            int tradeQty = (newNode->order.quantity < bestOpposite->quantity) ? 
                           newNode->order.quantity : bestOpposite->quantity;

            int expectedOpp = bestOpposite->quantity;
            while (expectedOpp >= tradeQty) {
                if (__sync_bool_compare_and_swap(&bestOpposite->quantity, expectedOpp, expectedOpp - tradeQty)) {
                    int expectedNew = newNode->order.quantity;
                    while (expectedNew >= tradeQty) {
                        if (__sync_bool_compare_and_swap(&newNode->order.quantity, expectedNew, expectedNew - tradeQty)) {
                            printf("Trade executed for ticker %s: %d shares at %.2f\n",
                                   newNode->order.ticker.c_str(), tradeQty, bestOpposite->price);
                            break;
                        }
                        expectedNew = newNode->order.quantity;
                    }
                    break;
                }
                expectedOpp = bestOpposite->quantity;
            }
        }
    }
};

Order* OrderBook::findBestOpposite(const Order& order, const OrderList& oppositeOrders) {
    Order* best = nullptr;
    double bestPrice = (order.orderType == BUY) ? MAX_DOUBLE : -1.0;
    OrderNode* node = const_cast<OrderNode*>(oppositeOrders.getHead());
    while (node) {
        Order& oppOrder = node->order;
        int qty = oppOrder.quantity;
        if (qty > 0) {
            if (order.orderType == BUY && oppOrder.price <= order.price) {
                if (oppOrder.price < bestPrice) {
                    bestPrice = oppOrder.price;
                    best = &oppOrder;
                }
            } else if (order.orderType == SELL && oppOrder.price >= order.price) {
                if (oppOrder.price > bestPrice) {
                    bestPrice = oppOrder.price;
                    best = &oppOrder;
                }
            }
        }
        node = const_cast<OrderNode*>(node->next);
    }
    return best;
}

// Global order books and utility functions
OrderBook* orderBooks = nullptr;

void initOrderBooks() {
    orderBooks = new OrderBook[NUM_TICKERS];
}

void cleanupOrderBooks() {
    delete[] orderBooks;
}

int getOrderBookIndex(const TickerString& ticker) {
    unsigned int hash = 0;
    for (int i = 0; ticker[i] != '\0'; i++) {
        hash = hash * 31 + static_cast<unsigned char>(ticker[i]);
    }
    return hash % NUM_TICKERS;
}

void addOrder(OrderType orderType, const TickerString& ticker, int quantity, double price) {
    int idx = getOrderBookIndex(ticker);
    Order order(orderType, ticker, quantity, price);
    orderBooks[idx].addOrder(order);
}

TickerString generateTickerSymbol(int index) {
    char buffer[MAX_TICKER_LENGTH];
    snprintf(buffer, MAX_TICKER_LENGTH, "TICKER%d", index);
    return TickerString(buffer);
}

TickerString* tickers = nullptr;

void initTickers() {
    tickers = new TickerString[NUM_TICKERS];
    for (int i = 0; i < NUM_TICKERS; i++) {
        tickers[i] = generateTickerSymbol(i);
    }
}

void cleanupTickers() {
    delete[] tickers;
}

// Simulation functions
void simulateTransactions(int numTransactions = 10) {
    for (int i = 0; i < numTransactions; i++) {
        OrderType orderType = (rng.randInt(0, 1) == 0) ? BUY : SELL;
        int tickerIndex = rng.randInt(0, NUM_TICKERS - 1);
        TickerString ticker = tickers[tickerIndex];
        int quantity = rng.randInt(1, 100);
        double price = rng.uniform(10.0, 100.0);
        price = (int)(price * 100) / 100.0; // Round to 2 decimal places
        addOrder(orderType, ticker, quantity, price);
    }
}

void brokerFunction(int brokerId, int iterations) {
    for (int i = 0; i < iterations; i++) {
        simulateTransactions(5);
    }
    printf("Broker %d completed activities\n", brokerId);
}

void runSimulation() {
    printf("Starting stock exchange simulation with threads...\n");
    initOrderBooks();
    initTickers();

    const int numBrokers = 5;
    std::thread brokerThreads[numBrokers];
    for (int i = 0; i < numBrokers; i++) {
        brokerThreads[i] = std::thread(brokerFunction, i, 200);
    }

    for (int i = 0; i < numBrokers; i++) {
        brokerThreads[i].join();
    }

    printf("Simulation completed\n");
    cleanupTickers();
    cleanupOrderBooks();
}

int main() {
    runSimulation();
    return 0;
}
/*
    Author: Prempreet Brar

    Program simulates secret bidding on an auctioned object using a main thread and
    n "bidder" threads; the rankings of the bidders are updated after each bid and
    displayed to the user in descending order, with the time taken for a bid used as a
    tie breaker if two bids have the same value.

    The bidders make temporary bids, "changing their mind," and have a 50/50 chance of
    committing a bid, until they hit the bid limit at which point they are forced to commit
    a bid. For each bid, the bidder is slept for a few seconds before committing a bid, 
    to simulate "thinking".
    
    Each bidder's bid value is displayed in real time using arrows, with a * used to indicate
    a committed bid and an > used to indicate the bid of a bidder who is undecided.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define MAX_BIDDERS 100
#define FALSE 0
#define TRUE 1

void userPrompt(int *numOfBidders, int *bidsPerBidder, int *maxBidValue, int *maxSleepTime);
void initializeAuction(int numOfBidders, int rankingSize, int bidsPerBidder, int maxBidValue, int maxSleepTime);
void createBidders(int numOfBidders);
void *bidding(void *bidderNumber);
void startAuction(int numOfBidders, int rankingSize);

void updateRanking(int rankingSize, int bidderNumber);
void display(int bidder, int value, int isDone);
void insertionSort(int rankingSize);
void sortHelper(int newIndex, int bid, int bidder, time_t time, int bidCount);

void displayAll(int numOfBidders);
void displayRanking(int numOfBidders);

void allocateBuffers(int numOfBidders, int rankingSize);
void freeMemory();
void checkAllocation(void *bufferPointer, char *bufferName);



struct shared {
    int bidsPerBidder;          // max # of bids per bidder
    int maxBidValue;
    int maxSleepTime;
    int auctionFinished;

    pthread_t *bidders;         // keep track of thread id's so every thread can be joined
    int *isRanked;              // keep track of which committed bids have already been ranked
    int *ranking;               // list of bids from highest to lowest
    int *rankedBidders;
    time_t *rankedTime;
    int *rankedBidCount;        // user can see the number of bids committed by each bidder

    int *bid;
    int *committed;
    time_t *bidTime;            // used as a tie-breaker if two bid values are the same
    int *bidCount;
};
struct shared auctionInfo;


int main() {
    int numOfBidders;
    int bidsPerBidder, maxBidValue, maxSleepTime;

    int rankingSize = 0;
    userPrompt(&numOfBidders, &bidsPerBidder, &maxBidValue, &maxSleepTime);
    initializeAuction(numOfBidders, rankingSize, bidsPerBidder, maxBidValue, maxSleepTime);
    createBidders(numOfBidders);
    startAuction(numOfBidders, rankingSize);
    
    displayAll(numOfBidders);
    // wait for all bidders to finish bidding before displaying the ranking
    for (int i = 0; i < numOfBidders; i++) pthread_join(auctionInfo.bidders[i], NULL);
    displayRanking(numOfBidders);
    freeMemory();
    exit(0);
}


void userPrompt(int *numOfBidders, int *bidsPerBidder, int *maxBidValue, int *maxSleepTime) {
    printf("How many bidders are in the auction (max 100)? ");
    scanf("%d", numOfBidders);

    // continue prompting for input until the number of bidders is <= 100
    while (*numOfBidders > MAX_BIDDERS) {
        printf("\nThe number of bidders must be less than or equal to 100.\nTry again: ");
        scanf("%d", numOfBidders);
    }

    printf("\nWhat is the maximum number of temporary bids allowed? ");
    scanf("%d", bidsPerBidder);

    printf("\nWhat is the maximum bid allowed (choose a reasonable value so the arrow can fit on your screen): ");
    scanf("%d", maxBidValue);

    printf("\nWhat is the maximum time (seconds) a bidder can take for a single bid? ");
    scanf("%d", maxSleepTime);
    // when doing the modulo for sleeping, it yields a value from 0 to maxSleepTime - 1, so incrementing allows the range
    // to be from 0 to maxSleepTime
    (*maxSleepTime)++;
}


void initializeAuction(int numOfBidders, int rankingSize, int bidsPerBidder, int maxBidValue, int maxSleepTime) {
    auctionInfo.bidsPerBidder = bidsPerBidder;
    auctionInfo.maxBidValue = maxBidValue;
    auctionInfo.maxSleepTime = maxSleepTime;
    auctionInfo.auctionFinished = FALSE;
    // the ranking must be dynamically allocated since the number of values in the ranked array is constantly changing
    allocateBuffers(numOfBidders, rankingSize);
    
    // pre-initialize all arrays to appropriate values
    for (int i = 0; i < numOfBidders; i++) auctionInfo.isRanked[i] = FALSE;
    for (int i = 0; i < numOfBidders; i++) auctionInfo.bid[i] = 0;
    for (int i = 0; i < numOfBidders; i++) auctionInfo.committed[i] = FALSE;
    for (int i = 0; i < numOfBidders; i++) auctionInfo.bidTime[i] = -1;
    for (int i = 0; i < numOfBidders; i++) auctionInfo.bidCount[i] = 0;
}


void createBidders(int numOfBidders) {
    // ensure that the bids made are random
    srand(time(NULL));
    int *auctionIds = (int *) malloc(sizeof(int) * numOfBidders);
    for (int i = 0; i < numOfBidders; i++) auctionIds[i] = i;

    // create all 100 bidders
    for (int i = 0; i < numOfBidders; i++) {
        pthread_t bidderId;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&bidderId, &attr, &bidding, (void *) &auctionIds[i]);
        auctionInfo.bidders[i] = bidderId;
    }
}


void *bidding(void *bidderNumber) {
    int id = *((int *)bidderNumber);
    while ((auctionInfo.bidCount[id] < auctionInfo.bidsPerBidder - 1) && (!auctionInfo.committed[id])) {
        int seconds = rand() % auctionInfo.maxSleepTime;
        sleep(seconds);
        
        auctionInfo.bid[id] = rand() % auctionInfo.maxBidValue;
        // bidder has a 50/50 chance of committing the bid
        int isCommitted = rand() % 2;
        auctionInfo.committed[id] = isCommitted;
        auctionInfo.bidCount[id]++;
    }
    // if the bidder has made upperBound - 1 bids without committing, then, the bidder must commit their final bid
    if (!auctionInfo.committed[id]) {
        auctionInfo.bid[id] = rand() % auctionInfo.maxBidValue;
        auctionInfo.committed[id] = TRUE;
        auctionInfo.bidCount[id]++;
    }

    // record the time the bidder committed their bid
    time_t timeSubmitted;
    auctionInfo.bidTime[id] = time(&timeSubmitted);
    pthread_exit(0);
}


void startAuction(int numOfBidders, int rankingSize) {
    while (!auctionInfo.auctionFinished) {
        // assume the auction is finished unless you find a bidder who has not committed a bid
        auctionInfo.auctionFinished = TRUE;
        for (int i = 0; i < numOfBidders; i++) {
            // if the bidder is already ranked then it doesn't matter if they have committed a bid
            if ((auctionInfo.committed[i]) && (!auctionInfo.isRanked[i])) {
                rankingSize++;
                updateRanking(rankingSize, i);

            } else if (!auctionInfo.committed[i])
                auctionInfo.auctionFinished = FALSE;
            
            // display the bidder's current bid value, and update the rankings
            display(i, auctionInfo.bid[i], auctionInfo.committed[i]);
            insertionSort(rankingSize);
        }  
        // wipe the screen to create the illusion of moving bids
        system("clear");
    }
}


void updateRanking(int rankingSize, int bidderNumber) {
    // increase the size of the buffer to hold one more element and then store a new element in it
    int *newRanking = (int *) realloc(auctionInfo.ranking, sizeof(int) * rankingSize);
    auctionInfo.ranking = newRanking;
    auctionInfo.ranking[rankingSize - 1] = auctionInfo.bid[bidderNumber];

    int *newRankedBidders = (int *) realloc(auctionInfo.rankedBidders, sizeof(int) * rankingSize);
    auctionInfo.rankedBidders = newRankedBidders;
    // put the bidder's number in the rankings
    auctionInfo.rankedBidders[rankingSize - 1] = bidderNumber;

    time_t *newRankedTime = (time_t *) realloc(auctionInfo.rankedTime, sizeof(time_t) * rankingSize);
    auctionInfo.rankedTime = newRankedTime;
    auctionInfo.rankedTime[rankingSize - 1] = auctionInfo.bidTime[bidderNumber];
    // the bidder is now ranked
    auctionInfo.isRanked[bidderNumber] = TRUE;

    int *newRankedBidCount = (int *) realloc(auctionInfo.rankedBidCount, sizeof(int) * rankingSize);
    auctionInfo.rankedBidCount = newRankedBidCount;
    auctionInfo.rankedBidCount[rankingSize - 1] = auctionInfo.bidCount[bidderNumber];
}


void display(int bidder, int value, int isDone) {
    // display dashes equal to the bidder's current bid value, if he is done bidding put a *, otherwise, put an >
    printf("%s%2d: ", "bidder", bidder);
    for (int i = 0; i < value; i++) printf("-");
    if (isDone) printf("*\n");
    else printf(">\n");
}


void displayAll(int numOfBidders) {
    for (int i = 0; i < numOfBidders; i++)
        display(i, auctionInfo.bid[i], auctionInfo.committed[i]);
    printf("\n");
}


void displayRanking(int numOfBidders) {
    printf("The winner of the auction is: bidder%d, with a bid of %d!\n\n", auctionInfo.rankedBidders[0], auctionInfo.ranking[0]);

    // display the ranking for each bidder in descending order
    printf("%14s%37s%39s", "Rank", " Time", "Number of Bids\n");
    for (int i = 0; i < numOfBidders; i++) {
        printf("Rank %3d  -  bidder%2d: %d \t\t(time: %d)\t\t[%d bid(s) submitted]\n", 
            i + 1, auctionInfo.rankedBidders[i], auctionInfo.ranking[i], auctionInfo.rankedTime[i], auctionInfo.rankedBidCount[i]);
        if (i % 10 == 9)
            // print out a boundary every 10 ranks to make it easier to see visually
            printf("----------------------------------------------------------------------------------------------\n");
    }
}


/* The following sorting algorithm was taken from "Introduction to Algorithms" by Cormen, Leiserson, Rivest, and Stein,
   commonly known as "CLRS." Minor modifications have been made to the algorithm." */
void insertionSort(int rankingSize) {
    // start at i = 1 instead of i = 0 since the leftmost element in the array cannot move left
    for (int i = 1; i < rankingSize; i++) {
        int keyBid = auctionInfo.ranking[i];
        int keyBidTime = auctionInfo.rankedTime[i];
        int keyBidder = auctionInfo.rankedBidders[i];
        int keyBidCount = auctionInfo.rankedBidCount[i];
        // begin looking at the elements to the left of the current "key"
        int j = i - 1;

        while ((j >= 0) && (auctionInfo.ranking[j] <= keyBid)) {
            // if the bid on the left is equal in value use time as a tiebreaker
            if ((auctionInfo.ranking[j] == keyBid) && (auctionInfo.rankedTime[j] <= keyBidTime))
                break;
            // if the bid at the current rank is smaller than the "key" bid, move it right 1 index (so it can move down in rank)
            sortHelper(j, auctionInfo.ranking[j], auctionInfo.rankedBidders[j], auctionInfo.rankedTime[j], auctionInfo.rankedBidCount[j]);
            j--;
        }
        // the "key" was replaced in all of its associated arrays. Every bidder in the while loop was moved right in the sort, so, the
        // last bidder that we moved exists twice (at an index k and one index to the right). Put the key at index k.
        sortHelper(j, keyBid, keyBidder, keyBidTime, keyBidCount);
    }
}


void sortHelper(int newIndex, int bid, int bidder, time_t time, int bidCount) {
    auctionInfo.ranking[newIndex + 1] = bid;
    auctionInfo.rankedBidders[newIndex + 1] = bidder;
    auctionInfo.rankedTime[newIndex + 1] = time;
    auctionInfo.rankedBidCount[newIndex + 1] = bidCount;
}

void allocateBuffers(int numOfBidders, int rankingSize) {
    auctionInfo.bidders = (pthread_t *) malloc((sizeof(pthread_t) * numOfBidders));
    checkAllocation((void *)auctionInfo.bidders, "bidders");

    // initialize the pointers to the buffers that hold ranked information
    auctionInfo.isRanked = (int *) malloc((sizeof(int) * numOfBidders));
    checkAllocation((void *)auctionInfo.isRanked, "isRanked");

    auctionInfo.ranking = (int *) malloc((sizeof(int) * rankingSize));
    checkAllocation((void *)auctionInfo.ranking, "ranking");

    auctionInfo.rankedBidders = (int *) malloc((sizeof(int) * rankingSize));
    checkAllocation((void *)auctionInfo.rankedBidders, "rankedBidders");

    auctionInfo.rankedTime = (time_t *) malloc((sizeof(time_t) * rankingSize));
    checkAllocation((void *)auctionInfo.rankedTime, "rankedTime");

    auctionInfo.rankedBidCount = (int *) malloc((sizeof(int) * rankingSize));
    checkAllocation((void *)auctionInfo.rankedBidCount, "rankedBidCount");

    // initialize the pointers to the buffers that hold unranked information
    auctionInfo.bid = (int *) malloc((sizeof(int) * numOfBidders));
    checkAllocation((void *)auctionInfo.bid, "bid");

    auctionInfo.committed = (int *) malloc((sizeof(int) * numOfBidders));
    checkAllocation((void *)auctionInfo.committed, "committed");

    auctionInfo.bidTime = (time_t *) malloc((sizeof(time_t) * numOfBidders));
    checkAllocation((void *)auctionInfo.bidTime, "bidTime");

    auctionInfo.bidCount = (int *) malloc((sizeof(int) * numOfBidders));
    checkAllocation((void *)auctionInfo.bidCount, "bidCount");
}


void freeMemory() {
    // free the allocated memory for all 10 buffers in the shared structure
    free(auctionInfo.bidCount);
    free(auctionInfo.bidTime);
    free(auctionInfo.committed);
    free(auctionInfo.bid);

    free(auctionInfo.rankedBidCount);
    free(auctionInfo.rankedTime);
    free(auctionInfo.rankedBidders);
    free(auctionInfo.ranking);

    free(auctionInfo.isRanked);
    free(auctionInfo.bidders);
}


void checkAllocation(void *bufferPointer, char *bufferName) {
    if (bufferPointer == NULL) {
        // if there is an error, print the name of the buffer where allocation failed,
        // then free memory before exiting
        fprintf(stderr, "Failed to allocate memory for %s", bufferName);
        freeMemory();   
        exit(-1);
    }
}
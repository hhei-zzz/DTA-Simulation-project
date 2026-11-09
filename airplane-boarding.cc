#include "ns3/core-module.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <string>
#include <iomanip>

using namespace ns3;
using namespace std;

// SETTINGS

const int NUMBER_OF_PASSENGERS = 120;
const int NUMBER_OF_ROWS = 20;
const double TICK = 0.5;


// PASSENGER

struct Passenger
{
    int id;
    int seatNumber;
    char seatColumn;

    // Random variables
    double walkTime; 
    double luggageTime;
    double seatTime;
    bool hasLuggage;

    // Simulation variables
    int currentRow;
    double remainingTime;

    bool seating;
    bool seated;
};

// GLOBAL SIMULATION DATA

vector<Passenger> passengers;
vector<int> boardingOrder;

// aisle[0] = entrance

vector<int> aisle(NUMBER_OF_ROWS + 1, -1);

// Keeps track of occupied seats
vector<vector<bool>> occupiedSeats(
    NUMBER_OF_ROWS + 1,
    vector<bool>(6, false)
);

int nextPassenger = 0;
int seatedPassengers = 0;

double totalBoardingTime = 0;


// CREATE PASSENGERS


void CreatePassengers()
{
    passengers.clear();

    random_device rd;
    mt19937 randomGenerator(rd());

    uniform_real_distribution<double> walkRandom(0.5, 1.0);
    uniform_real_distribution<double> luggageRandom(5.0, 15.0);
    uniform_real_distribution<double> seatRandom(2.0, 5.0);
    bernoulli_distribution luggageChance(0.8);


    for (int i = 0; i < NUMBER_OF_PASSENGERS; i++)
    {
        Passenger passenger;

        passenger.id = i + 1;

        passenger.seatNumber = (i / 6) + 1;
        passenger.seatColumn = 'A' + (i % 6);

        passenger.walkTime =
            walkRandom(randomGenerator);

        passenger.hasLuggage =
            luggageChance(randomGenerator);

        if (passenger.hasLuggage)
        {
            passenger.luggageTime =
                luggageRandom(randomGenerator);
        }
        else
        {
            passenger.luggageTime = 0;
        }

        passenger.seatTime =
            seatRandom(randomGenerator);

        passenger.currentRow = 0;
        passenger.remainingTime = 0;

        passenger.seating = false;
        passenger.seated = false;

        passengers.push_back(passenger);
    }
}

int SeatIndex(char column)
{
    return column - 'A';
}

// RANDOM BOARDING

vector<int> RandomBoarding()
{
    vector<int> order(NUMBER_OF_PASSENGERS);

    for (int i = 0; i < NUMBER_OF_PASSENGERS; i++)
    {
        order[i] = i;
    }

    random_device rd;
    mt19937 randomGenerator(rd());

    shuffle(
        order.begin(),
        order.end(),
        randomGenerator
    );

    return order;
}


// FRONT TO BACK


vector<int> FrontToBack()
{
    vector<int> order = RandomBoarding();

    stable_sort(
        order.begin(),
        order.end(),

        [](int a, int b)
        {
            return passengers[a].seatNumber
                 < passengers[b].seatNumber;
        }
    );

    return order;
}


// BACK TO FRONT


vector<int> BackToFront()
{
    vector<int> order = RandomBoarding();

    stable_sort(
        order.begin(),
        order.end(),

        [](int a, int b)
        {
            return passengers[a].seatNumber
                 > passengers[b].seatNumber;
        }
    );

    return order;
}


// WILMA

int WilmaPriority(char seat)
{
    if (seat == 'A' || seat == 'F')
    {
        return 0;
    }

    if (seat == 'B' || seat == 'E')
    {
        return 1;
    }

    return 2;
}


vector<int> Wilma()
{
    vector<int> order = RandomBoarding();

    stable_sort(
        order.begin(),
        order.end(),

        [](int a, int b)
        {
            return WilmaPriority(
                       passengers[a].seatColumn
                   )
                 <
                   WilmaPriority(
                       passengers[b].seatColumn
                   );
        }
    );

    return order;
}

// STEFFEN METHOD

void AddSteffenSeats(
    vector<int>& order,
    char seat,
    bool evenRows
)
{
    for (int row = NUMBER_OF_ROWS;
         row >= 1;
         row--)
    {
        if ((row % 2 == 0) == evenRows)
        {
            int index =
                (row - 1) * 6
                + SeatIndex(seat);

            order.push_back(index);
        }
    }
}


vector<int> Steffen()
{
    vector<int> order;

    // Window seats
    AddSteffenSeats(order, 'A', true);
    AddSteffenSeats(order, 'F', true);

    AddSteffenSeats(order, 'A', false);
    AddSteffenSeats(order, 'F', false);


    // Middle seats
    AddSteffenSeats(order, 'B', true);
    AddSteffenSeats(order, 'E', true);

    AddSteffenSeats(order, 'B', false);
    AddSteffenSeats(order, 'E', false);


    // Aisle seats
    AddSteffenSeats(order, 'C', true);
    AddSteffenSeats(order, 'D', true);

    AddSteffenSeats(order, 'C', false);
    AddSteffenSeats(order, 'D', false);


    return order;
}

// SEAT INTERFERENCE

double SeatInterference(
    Passenger& passenger
)
{
    int row = passenger.seatNumber;

    double delay = 0;

    const double EXTRA_DELAY = 2.0;


    if (passenger.seatColumn == 'A')
    {
        if (occupiedSeats[row][1])
            delay += EXTRA_DELAY;

        if (occupiedSeats[row][2])
            delay += EXTRA_DELAY;
    }


    else if (passenger.seatColumn == 'B')
    {
        if (occupiedSeats[row][2])
            delay += EXTRA_DELAY;
    }


    else if (passenger.seatColumn == 'E')
    {
        if (occupiedSeats[row][3])
            delay += EXTRA_DELAY;
    }


    else if (passenger.seatColumn == 'F')
    {
        if (occupiedSeats[row][3])
            delay += EXTRA_DELAY;

        if (occupiedSeats[row][4])
            delay += EXTRA_DELAY;
    }


    return delay;
}

// SIMULATION TICK

void SimulationTick()
{
    for (int row = NUMBER_OF_ROWS;
         row >= 0;
         row--)
    {
        int passengerIndex = aisle[row];

        if (passengerIndex == -1)
        {
            continue;
        }


        Passenger& passenger =
            passengers[passengerIndex];

        if (passenger.remainingTime > 0)
        {
            passenger.remainingTime -= TICK;

            if (passenger.remainingTime > 0)
            {
                continue;
            }

            passenger.remainingTime = 0;
        }


        if (passenger.seating)
        {
            passenger.seating = false;
            passenger.seated = true;

            occupiedSeats
                [passenger.seatNumber]
                [SeatIndex(passenger.seatColumn)]
                = true;

            aisle[row] = -1;

            seatedPassengers++;

            continue;
        }


        if (passenger.currentRow ==
            passenger.seatNumber)
        {
            passenger.seating = true;

            passenger.remainingTime =
                passenger.luggageTime
                + passenger.seatTime
                + SeatInterference(passenger);

            continue;
        }


        // Try to move forward

        int nextRow =
            passenger.currentRow + 1;


        if (nextRow <= NUMBER_OF_ROWS &&
            aisle[nextRow] == -1)
        {
            aisle[passenger.currentRow] = -1;

            aisle[nextRow] =
                passengerIndex;

            passenger.currentRow =
                nextRow;

            passenger.remainingTime =
                passenger.walkTime;
        }
    }


    // Let next passenger enter

    if (nextPassenger <
            NUMBER_OF_PASSENGERS
        &&
        aisle[0] == -1)
    {
        int passengerIndex =
            boardingOrder[nextPassenger];

        aisle[0] =
            passengerIndex;

        passengers[passengerIndex].currentRow = 0;

        passengers[passengerIndex].remainingTime =
            passengers[passengerIndex].walkTime;

        nextPassenger++;
    }


    // All passengers seated

    if (seatedPassengers ==
        NUMBER_OF_PASSENGERS)
    {
        totalBoardingTime =
            Simulator::Now().GetSeconds();

        Simulator::Stop();

        return;
    }


    // Run next tick

    Simulator::Schedule(
        Seconds(TICK),
        &SimulationTick
    );
}

// RUN ONE BOARDING METHOD

double RunSimulation(
    string method,
    const vector<Passenger>& passengerBatch
)
{
    // Copy the same original batch for this method
    passengers = passengerBatch;

    if (method == "Random")
    {
        boardingOrder = RandomBoarding();
    }

    else if (method == "Front-to-Back")
    {
        boardingOrder = FrontToBack();
    }

    else if (method == "Back-to-Front")
    {
        boardingOrder = BackToFront();
    }

    else if (method == "WILMA")
    {
        boardingOrder = Wilma();
    }

    else if (method == "Steffen")
    {
        boardingOrder = Steffen();
    }


    // Reset airplane
    aisle.assign(
        NUMBER_OF_ROWS + 1,
        -1
    );

    occupiedSeats.assign(
        NUMBER_OF_ROWS + 1,
        vector<bool>(6, false)
    );

    nextPassenger = 0;
    seatedPassengers = 0;
    totalBoardingTime = 0;


    Simulator::ScheduleNow(
        &SimulationTick
    );

    Simulator::Run();
    Simulator::Destroy();


    return totalBoardingTime;
}

// MAIN

int main(int argc, char* argv[])
{
    vector<string> methods =
    {
        "Random",
        "Front-to-Back",
        "Back-to-Front",
        "WILMA",
        "Steffen"
    };


    cout << fixed
         << setprecision(1);


    // Create ONE batch of 120 passengers
    CreatePassengers();

    // Save that original batch
    vector<Passenger> passengerBatch =
        passengers;

    // Every method uses the SAME batch
    for (string method : methods)
    {
        double time =
            RunSimulation(
                method,
                passengerBatch
            );

        cout
            << method
            << ": "
            << time
            << " seconds"
            << endl;
    }

    return 0;
}
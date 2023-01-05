#ifndef TYPES_H
#define TYPES_H

typedef struct bike_message {
    double speed;
    double cadence;
    double distance;

} bike_message;

static char* SPEED_QUERY = "SELECT median(\"value\") FROM \"speed\" WHERE time >= now() - 90m and time <= now() GROUP BY time(5s) fill(0) ORDER BY time DESC LIMIT 6";
static char* CADENCE_QUERY = "SELECT median(\"value\") FROM \"cadence\" WHERE time >= now() - 90m and time <= now() GROUP BY time(5s) fill(0) ORDER BY time DESC LIMIT 6";
static char* DISTANCE_QUERY = "SELECT integral(\"value\") FROM \"speed\" WHERE time >= now() - 90m and time <= now()";

#endif
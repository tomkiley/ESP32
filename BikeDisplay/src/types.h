#ifndef TYPES_H
#define TYPES_H

typedef struct bike_message {
    double speed;
    double cadence;
    double distance;

} bike_message;

// static char* SPEED_QUERY = "SELECT median(value) FROM speed WHERE time >= now() - 90m and time <= now() GROUP BY time(\"5s\") fill(0) ORDER BY time DESC LIMIT 6";
// static char* CADENCE_QUERY = "SELECT median(value) FROM cadence WHERE time >= now() - 90m and time <= now() GROUP BY time(\"5s\") fill(0) ORDER BY time DESC LIMIT 6";
// static char* DISTANCE_QUERY = "SELECT integral(value) FROM speed WHERE time >= now() - 90m and time <= now()";

char* CADENCE_QUERY = "from(bucket: \"bike-test\")"
    "|> range(start: -2h)"
    "|> filter(fn: (r) => r._measurement == \"cadence\" and  r._field == \"value\")"
    "|> aggregateWindow(every: 5s, fn: median)"
    "|> fill(value: 0.0)"
    "|> movingAverage(n: 3)"
    "|> last()";

char* SPEED_QUERY = "from(bucket: \"bike-test\")"
    "|> range(start: -2h)"
    "|> filter(fn: (r) => r._measurement == \"speed\" and  r._field == \"value\")"
    "|> aggregateWindow(every: 5s, fn: median)"
    "|> fill(value: 0.0)"
    "|> movingAverage(n: 3)"
    "|> last()";

char* DISTANCE_QUERY = "from(bucket: \"bike-test\")"
    "|> range(start: -12h)"
    "|> filter(fn: (r) => r._measurement == \"speed\" and  r._field == \"value\")"
    "|> integral(unit:  1h)";

#endif
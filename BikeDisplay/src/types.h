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
    "|> range(start: -5m)"
    "|> filter(fn: (r) => r._measurement == \"cadence\" and  r._field == \"value\")"
    "|> aggregateWindow(every: 1s, fn: median)"
    "|> fill(value: 0.0)"
    "|> exponentialMovingAverage(n: 3)"
    "|> last()";

char* SPEED_QUERY = "from(bucket: \"bike-test\")"
    "|> range(start: -5m)"
    "|> filter(fn: (r) => r._measurement == \"speed\" and  r._field == \"value\")"
    "|> aggregateWindow(every: 1s, fn: median)"
    "|> fill(value: 0.0)"
    "|> exponentialMovingAverage(n: 3)"
    "|> last()";

char* DISTANCE_QUERY = "from(bucket: \"bike-test\")"
    "|> range(start: -4h)"
    "|> filter(fn: (r) => r._measurement == \"speed\" and  r._field == \"value\")"
    "|> integral(unit:  1h)";


char* COMBINED = ""
"a = from(bucket: \"bike-test\")"
    "|> range(start: -5m)"
    "|> filter(fn: (r) => r._measurement == \"cadence\" and  r._field == \"value\")"
    "|> exponentialMovingAverage(n: 12)"
    "|> last()"
    "|> set(key: \"tag\", value: \"cadence\")"
"b = from(bucket: \"bike-test\")"
    "|> range(start: -5m)"
    "|> filter(fn: (r) => r._measurement == \"speed\" and  r._field == \"value\")"
    "|> exponentialMovingAverage(n: 12)"
    "|> last()"
    "|> set(key: \"tag\", value: \"speed\")"
"c = from(bucket: \"bike-test\")"
    "|> range(start: -4h)"
    "|> filter(fn: (r) => r._measurement == \"speed\" and  r._field == \"value\")"
    "|> integral(unit:  1h)"
    "|> set(key: \"tag\", value: \"distance\")"
"d = from (bucket: \"home_assistant\")"
    "|> range(start: -2h)"
    "|> filter (fn: (r) => r.domain == \"media_player\" and r.entity_id == \"shield_2\")"
    "|> filter (fn: (r) => r._field == \"state\" or r._field == \"media_position\")"
    "|> last()"
    "|> set(key: \"tag\", value: \"mp_state\")"
"e = from (bucket: \"home_assistant\")"
    "|> range(start: -2h)"
    "|> filter (fn: (r) => r.domain == \"media_player\" and r.entity_id == \"shield_2\")"
    "|> filter (fn: (r) => r._field == \"media_position\")"
    "|> last()"
    "|> set(key: \"tag\", value: \"mp_pos\")"
"union(tables: [a,b,c,d,e])"
    ;

#endif
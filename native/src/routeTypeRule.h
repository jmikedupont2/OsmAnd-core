#ifndef _OSMAND_ROUTE_TYPE_RULE_H_
#define _OSMAND_ROUTE_TYPE_RULE_H_

#include <string>
#include <vector>
#include <ctime>

#include "openingHoursParser.h"

struct RouteTypeCondition {
    std::string condition;
    std::shared_ptr<OpeningHoursParser::OpeningHours> hours;
    std::string value;
    uint32_t ruleid;
    RouteTypeCondition() : condition(""), hours(nullptr), value(""), ruleid(0) {
    }
};

struct RouteTypeRule {
    const static int ACCESS = 1;
    const static int ONEWAY = 2;
    const static int HIGHWAY_TYPE = 3;
    const static int MAXSPEED = 4;
    const static int ROUNDABOUT = 5;
    const static int TRAFFIC_SIGNALS = 6;
    const static int RAILWAY_CROSSING = 7;
    const static int LANES = 8;
    const static int TUNNEL = 9;
    
private:
    std::string t;
    std::string v;
    int intValue;
    float floatValue;
    int type;
    std::vector<RouteTypeCondition> conditions;
    int forward;
    
private:
    void analyze();
    
public:
    RouteTypeRule() : t(""), v("") {
    }
    
    RouteTypeRule(std::string t, std::string v) : t(t) {
        if ("true" == v) {
            v = "yes";
        }
        if ("false" == v) {
            v = "no";
        }
        this->v = v;
        this->analyze();
    }
    
    inline int isForward() {
        return forward;
    }
    
    inline const std::string& getTag() {
        return t;
    }
    
    inline const std::string& getValue() {
        return v;
    }

    inline std::vector<RouteTypeCondition>& getConditions() {
        return conditions;
    }
    
    inline const std::string getNonConditionalTag() {
        int ind = t.find(":conditional");
        if (ind >= 0) {
            return t.substr(0, ind);
        }
        return t;
    }

    inline bool roundabout() {
        return type == ROUNDABOUT;
    }
    
    inline int getType() {
        return type;
    }
    
    inline bool conditional() {
        return !conditions.empty();
    }
    
    inline int onewayDirection() {
        if (type == ONEWAY){
            return intValue;
        }
        return 0;
    }
    
    float maxSpeed();

    const uint32_t conditionalValue(const tm& dateTime);
    
    inline int lanes() {
        if (type == LANES) {
            return intValue;
        }
        return -1;
    }
    
    inline std::string highwayRoad() {
        if (type == HIGHWAY_TYPE) {
            return v;
        }
        return "";
    }
};

#endif

#ifndef FILTERS_H
#define FILTERS_H

class EwmaFilter {
private:
    float alpha;
    float filteredValue;
    bool initialized;

public:
    EwmaFilter(float filterAlpha) : alpha(filterAlpha), filteredValue(0.0f), initialized(false) {}

    float update(float input) {
        if (!initialized) {
            filteredValue = input;
            initialized = true;
        } else {
            filteredValue = (alpha * input) + ((1.0f - alpha) * filteredValue);
        }
        return filteredValue;
    }

    float getValue() const {
        return filteredValue;
    }
};

#endif
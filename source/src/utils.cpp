#include "utils.h"
#include "common.h"

#if COIN_THING_SERIAL > 0
int callDepth { 0 };
uint32_t lastIndentMillis { 0 };
#endif

String urlencode(const String& url)
{
    String encoded = "";
    char c;
    char code0;
    char code1;
    for (auto i = 0; i < url.length(); ++i) {
        c = url.charAt(i);
        if (c == ' ') {
            encoded += '+';
        } else if (isalnum(c)) {
            encoded += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
        yield();
    }
    return encoded;
}

bool isCompactSmall(CompressNumberFormat cnf)
{
    return (cnf == CompressNumberFormat::COMPACT_SMALL
        || cnf == CompressNumberFormat::COMPACT_SMALL_AND_LARGE);
}

bool isCompactLarge(CompressNumberFormat cnf)
{
    return (cnf == CompressNumberFormat::COMPACT_LARGE
        || cnf == CompressNumberFormat::COMPACT_SMALL_AND_LARGE);
}

void formatNumber(gecko_t n,
    String& s,
    NumberFormat format,
    bool forceSign,
    bool dash00,
    CompressNumberFormat compressNumberFormat,
    uint8_t forceDecimalPlaces,
    bool lessDecimalPlaces)
{
    char buf[21];
    char buf2[21];

    uint8_t decimalPlaces(forceDecimalPlaces);
    gecko_t absoluteValue((n < 0 ? n * -1. : n));
    uint8_t compactFormatPlaces(4); // fix number of digits after .#z 1234

    if (forceDecimalPlaces == std::numeric_limits<uint8_t>::max()) {
        decimalPlaces = 4;
        if (lessDecimalPlaces) {
            decimalPlaces = 2;
        }

        if (absoluteValue < 0.000000001) {
            decimalPlaces = 13;
        } else if (absoluteValue < 0.00000001) {
            decimalPlaces = 12;
        } else if (absoluteValue < 0.0000001) {
            decimalPlaces = 11;
        } else if (absoluteValue < 0.000001) {
            decimalPlaces = 10;
        } else if (absoluteValue < 0.00001) {
            decimalPlaces = 9;
        } else if (absoluteValue < 0.0001) {
            decimalPlaces = 8;
        } else if (absoluteValue < 0.001) {
            decimalPlaces = 7;
        } else if (absoluteValue < 0.01) {
            decimalPlaces = 6;
        } else if (absoluteValue < 1.) {
            decimalPlaces = 5;
            if (lessDecimalPlaces) {
                decimalPlaces = 3;
            }
        } else if (static_cast<uint32_t>(absoluteValue) > 99999) {
            decimalPlaces = 2;
        }

        if (lessDecimalPlaces
            && absoluteValue < 0.01) {
            if (isCompactSmall(compressNumberFormat)) {
                decimalPlaces -= 1;
                compactFormatPlaces = 3;
            } else {
                decimalPlaces -= 2;
            }
        }
    }

    String largeNumberPostfix;
    if (isCompactLarge(compressNumberFormat)) {
        if (absoluteValue >= 1000000000000) { // trillion
            n /= 1000000000000.;
            decimalPlaces = 3;
            largeNumberPostfix = 'T';
        } else if (absoluteValue >= 1000000000) { // billion
            n /= 1000000000.;
            decimalPlaces = 3;
            largeNumberPostfix = 'B';
        } else if (absoluteValue >= 1000000) { // million
            n /= 1000000.;
            decimalPlaces = 3;
            largeNumberPostfix = 'M';
        }
    }

    if (forceSign && n != 0.) {
        snprintf(buf, sizeof(buf), "%+.*f", decimalPlaces, n);
    } else {
        snprintf(buf, sizeof(buf), "%.*f", decimalPlaces, n);
    }

    char thousandSeparator('#'); // means no separator
    char decimalSeparator('.');

    if (format == NumberFormat::THOUSAND_DOT_DECIMAL_COMMA) {
        thousandSeparator = '.';
        decimalSeparator = ',';
    } else if (format == NumberFormat::THOUSAND_BLANK_DECIMAL_COMMA) {
        thousandSeparator = ' ';
        decimalSeparator = ',';
    } else if (format == NumberFormat::DECIMAL_COMMA) {
        decimalSeparator = ',';
    } else if (format == NumberFormat::THOUSAND_COMMA_DECIMAL_DOT) {
        thousandSeparator = ',';
        decimalSeparator = '.';
    } else if (format == NumberFormat::THOUSAND_BLANK_DECIMAL_DOT) {
        thousandSeparator = ' ';
        decimalSeparator = '.';
    }

    bool hasDecSep(false);
    int rev(0);
    for (auto pos = strlen(buf); pos > 0; --pos, ++rev) {
        char c = buf[pos - 1];
        if (c == '.') {
            hasDecSep = true;
        }
        buf2[rev] = c;
    }
    buf2[rev] = 0; // buf2 holds number string in reverse order

    uint8_t numTrailingZeroes(0);
    uint8_t posDecSep(0);
    if (hasDecSep) {
        for (; buf2[numTrailingZeroes] == '0'; ++numTrailingZeroes) { }
        for (posDecSep = numTrailingZeroes; buf2[posDecSep] != '.'; ++posDecSep) { }
    }

    bool reachedDecSep(false);
    uint8_t copyTo(sizeof(buf) - 1);
    uint8_t group(0);

    buf[copyTo] = 0;
    --copyTo;

    uint8_t copyFrom(0);
    if (numTrailingZeroes > 0 && largeNumberPostfix.isEmpty()) {
        // remove trailing zeroes (after the decimal separator)
        //  but leave at least 2 digits
        copyFrom = min<uint8_t>(numTrailingZeroes, posDecSep - 2);
    }

    // reverse copy the numeric string and apply some formatting
    for (; buf2[copyFrom] != 0 && copyTo != 0; ++copyFrom) {
        char c = buf2[copyFrom];

        if (!hasDecSep || reachedDecSep) {
            ++group;
        }

        if (c == '.') {
            c = decimalSeparator;
            reachedDecSep = true;
        }

        buf[copyTo] = c;
        --copyTo;

        if (group > 0
            && group % 3 == 0
            && buf2[copyFrom + 1] != 0
            && buf2[copyFrom + 1] != '+') {
            if (thousandSeparator != '#') {
                buf[copyTo] = thousandSeparator;
                --copyTo;
            }
        }
    }
    s = &buf[copyTo + 1];

    if (isCompactSmall(compressNumberFormat)
        && n != 0.
        && absoluteValue < 0.01) {
        String pattern;
        String sign;
        if (s[0] == '+' || s[0] == '-') {
            sign = s[0];
            pattern = s[0];
        }
        pattern += F("0");
        pattern += decimalSeparator;
        pattern += F("00");
        uint8_t zeroesAfterDecSep(2);
        bool compactNotation(false);
        while (s.indexOf(pattern) == 0 && pattern.length() < s.length()) {
            pattern += F("0");
            ++zeroesAfterDecSep;
            compactNotation = true;
        }
        if (compactNotation) {
            String z(sign);
            z += decimalSeparator;
            z += (--zeroesAfterDecSep);
            z += F("z");
            if (thousandSeparator != '#') {
                z += F(" ");
            }
            pattern = pattern.substring(0, pattern.length() - 1);
            s.replace(pattern, z);

            // s = "+.4z 123" --> "+.4z 1230"
            uint8_t rpad(compactFormatPlaces - (s.length() - z.length()));
            for (; rpad > 0; --rpad) {
                s += '0';
            }
        }
    }

    if (dash00) {
        String dotZero(decimalSeparator);
        dotZero += F("00");
        if (s.endsWith(dotZero)) {
            String repl;
            repl = decimalSeparator;
            repl += F("\u2012"); // Figure Dash U+2012
            s.replace(dotZero, repl);
        }
    }

    if (!largeNumberPostfix.isEmpty()) {
        if (thousandSeparator != '#') {
            s += F(" ");
        }
        s += largeNumberPostfix;
    }

    s.replace(F(" "), F("\u2006")); // Six-Per-Em Space U+2006
}

void addCurrencySmbol(String& value, const String& symbol, CurrencySymbolPosition position)
{
    if (position == CurrencySymbolPosition::LEADING) {
        value = symbol + value;
    } else {
        value += symbol;
    }
}

uint32_t millis_test()
{
#if 0
    uint32_t ret(millis() + (0xffffffff - 10000));
    static uint32_t last(ret);
    if (ret < last) {
        LOG_PRINTLN("millis() overflow");
    }
    last = ret;
    return ret;
#else
    return millis();
#endif
}

bool doInterval(uint32_t change, uint32_t interval)
{
    if (change == 0) {
        return true;
    }
    if ((millis_test() - change) >= interval) {
        return true;
    }
    return false;
}

#include "gtest/gtest.h"
#include <string>

extern "C" {
#include "esp_utils.h"
}

#define SUBSTR_TEST(S1, S2, P) \
    ASSERT_EQ(esp_find_substr((S1), strlen((S1)), (S2), strlen((S2))), (P));

TEST(test_esp_modem, substring_test_simple)
{
    const char *hay = "one\rtwo\r\nthree";

    const char *str1 = "one";
    SUBSTR_TEST(hay, str1, &hay[0]);

    const char *str2 = "two";
    SUBSTR_TEST(hay, str2, &hay[4]);

    const char *str3 = "three";
    SUBSTR_TEST(hay, str3, &hay[9]);
}

TEST(test_esp_modem, substring_test_corner)
{
    const char *hay = "one";
    size_t hay_len = strlen(hay);

    const char *str1 = "one";
    SUBSTR_TEST(hay, str1, &hay[0]);

    const char *str2 = "two";
    SUBSTR_TEST(hay, str2, nullptr);

    const char *str3 = "four";
    SUBSTR_TEST(hay, str3, nullptr);

    ASSERT_EQ(esp_find_substr(hay, 0, str1, strlen(str1)), nullptr);
    ASSERT_EQ(esp_find_substr(hay, strlen(hay), str1, 0), nullptr);
}

TEST(test_esp_modem, substring_test_nulls)
{
    char hay[128] = {0};
    strncpy(hay, "one\rtwo\r\nthree", 128);
    size_t hay_len = strlen(hay);
    hay[3] = '\0';

    ASSERT_EQ(esp_find_substr(hay, hay_len, "one", 3), &hay[0]);
    ASSERT_EQ(esp_find_substr(hay, hay_len, "one\0", 4), &hay[0]);
    ASSERT_EQ(esp_find_substr(hay, hay_len, "two", 3), &hay[4]);
    ASSERT_EQ(esp_find_substr(hay, hay_len, "two\0", 4), nullptr);
}

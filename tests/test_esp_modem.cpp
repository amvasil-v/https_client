#include "gtest/gtest.h"
#include <vector>
#include <string>

extern "C" {
#include "esp_modem.h"
}

TEST(test_esp_modem, platform_test)
{
    ASSERT_EQ(sizeof(void*), 8);
}

TEST(test_esp_modem, tokenize_string)
{
    esp_string_list_t *res, *check;
    char buf[256] = {0};
    
    res = esp_string_tokenize(NULL, NULL);
    ASSERT_EQ(res, (void*)NULL);
    esp_string_list_free(res);

    const char *str1 = "1\r\n22\r\n333\r\n\r\n4444\r\n55555\r\n\r\n";
    res = check = esp_string_tokenize(str1, "\r\n");
    std::vector<const char *> vect{"1","22","333","4444","55555"};
    for (const auto& v: vect) {
        std::string help_str = " comparing " +
                std::string(v) + " vs " + std::string(check->str).substr(0, strlen(v));
        ASSERT_NE(check, (void *)NULL) << help_str;
        ASSERT_NE(check->str, (void *)NULL) << help_str;
        ASSERT_EQ(strlen(v), check->len) << help_str;
        ASSERT_STREQ(std::string(check->str).substr(0, check->len).c_str(), v) << help_str;
        check = check->next;
    }
    ASSERT_EQ(check, (void *)NULL);
    esp_string_list_free(res);

    const char *str2 = "\r\n";
    res = esp_string_tokenize(NULL, NULL);
    ASSERT_EQ(res, (void*)NULL);
    esp_string_list_free(res);
}

TEST(test_esp_modem, append_complete)
{
    constexpr size_t buf_len = 256;
    char buf[buf_len];
    char *append = buf;
    EXPECT_EQ(esp_append_check_complete(buf, &append, buf_len, "1111\r\n"), 0);
    EXPECT_EQ(esp_append_check_complete(buf, &append, buf_len, "OK\r\n"), 1);
    ASSERT_STREQ(buf, "1111\r\nOK\r\n");
}
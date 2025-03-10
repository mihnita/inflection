/*
 * Copyright 2017-2024 Apple Inc. All rights reserved.
 */
#pragma once

#include <morphuntion/dialog/fwd.hpp>
#include <morphuntion/grammar/synthesis/fwd.hpp>
#include <string>

class morphuntion::grammar::synthesis::NlGrammarSynthesizer final
{
public:
    static void addSemanticFeatures(::morphuntion::dialog::SemanticFeatureModel& featureModel);
public:
    static bool isVowelAtIndex(std::u16string_view str, int32_t index);
    static void guessDiminutive(::std::u16string* result, std::u16string_view guess);
public:

    enum class Count {
        undefined,
        singular,
        plural
    };
    static Count getCount(const ::std::u16string* value);

    enum class Gender {
        undefined,
        masculine,
        feminine,
        neuter
    };
    static Gender getGender(const ::std::u16string* value);

    enum class Declension {
        undefined,
        declined,
        undeclined
    };
    static Declension getDeclension(const ::std::u16string* value);

    enum class Case {
        undefined,
        genitive,
        nominative
    };
    static Case getCase(const ::std::u16string* value);

private:
    NlGrammarSynthesizer() = delete;

public:
    static const ::std::u16string& SIZENESS();
    static const ::std::u16string& DECLENSION();
    static const ::std::u16string& DECLENSION_UNDECLINED();
    static const ::std::u16string& DECLENSION_DECLINED();
    static const ::std::u16string& SIZENESS_DIMINUTIVE();
    static constexpr auto DE = u"de";
    static constexpr auto DEZE = u"deze";
    static constexpr auto DIE = u"die";
    static constexpr auto ARTICLE_DEFINITE = u"defArticle";
    static constexpr auto ARTICLE_INDEFINITE = u"indefArticle";
};

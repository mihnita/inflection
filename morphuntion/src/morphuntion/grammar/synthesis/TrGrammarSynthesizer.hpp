/*
 * Copyright 2017-2024 Apple Inc. All rights reserved.
 */
#pragma once

#include <morphuntion/dictionary/fwd.hpp>
#include <icu4cxx/UnicodeSet.hpp>
#include <morphuntion/dialog/fwd.hpp>
#include <morphuntion/grammar/synthesis/fwd.hpp>
#include <string>

class morphuntion::grammar::synthesis::TrGrammarSynthesizer final
{
public:
    static void addSemanticFeatures(::morphuntion::dialog::SemanticFeatureModel& featureModel);

    enum class VowelGroup {
        // UNDEFINED, // Impossible AKA @Nonnull
        FRONT_UNROUNDED,
        FRONT_ROUNDED,
        BACK_UNROUNDED,
        BACK_ROUNDED
    };
    static char16_t getAffix(VowelGroup value);
    enum class Case {
        undefined,
        ABLATIVE,
        ACCUSATIVE,
        DATIVE,
        GENITIVE,
        LOCATIVE,
        INSTRUMENTAL
    };
    static Case getCase(const ::std::u16string* str);
    enum class Count {
        undefined,
        SINGULAR,
        PLURAL
    };
    static Count getCount(const ::std::u16string* str);
    enum class Person {
        undefined,
        FIRST,
        SECOND,
        THIRD
    };
    static Person getPerson(const ::std::u16string* str);
    enum class Pronoun {
        undefined,
        SINGULAR,
        PLURAL
    };
    static Pronoun getPronoun(const ::std::u16string* str);
    enum class Copula {
        undefined,
        FIRST,
        SECOND,
        THIRD
    };
    static Copula getCopula(const ::std::u16string* str);
    enum class Tense {
        undefined,
        present,
        past
    };
    static Tense getTense(const ::std::u16string* str);

private:
    TrGrammarSynthesizer() = delete;

public:
    static const ::std::u16string& PRONOUN();
    static const ::std::u16string& PRONOUN_NUMBER();
    static const ::std::u16string& COPULA();
    static const ::std::u16string& TENSE();
    static const ::std::u16string& COPULA_FIRST();
    static const ::std::u16string& COPULA_SECOND();
    static const ::std::u16string& COPULA_THIRD();
    static const ::std::u16string& TENSE_PRESENT();
    static const ::std::u16string& TENSE_PAST();
    static constexpr auto INTERROGATIVE_ARTICLE = u"interrogativeArticle";
    static constexpr auto DE_CONJUNCTION = u"deConjunction";
    static const ::icu4cxx::UnicodeSet& SEPARATOR_SPLITTER();
    static const ::icu4cxx::UnicodeSet& ENDING_SINGLE_QUOTE();
    static const ::icu4cxx::UnicodeSet& ENDING_DOUBLE_QUOTE();
};

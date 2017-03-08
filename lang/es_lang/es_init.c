#include "cst_ffeatures.h"
#include "es_lang.h"

#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <Saga.h>

typedef struct convert_table_s {
    char * sampa;
    char * arpabet;
} convert_t;

convert_t convert_table[] = {
    {"tS", "ch"},
    {"jj", ""}, // IPA y
    {"rr", "r"},
    {"dZ", "jh"}, // IPA d3
    {"p", "p"},
    {"b", "b"},
    {"t", "t"},
    {"d", "d"},
    {"k", "k"},
    {"g", "g"},
    {"m", "m"},
    {"n", "n"}, 
    {"J", "eng"}, // ???? IPA backwards ŋ
    {"N", "ng"}, //IPA ŋ
    {"B", ""}, // IPA beta
    {"f", "f"},
    {"T", "th"}, // IPA θ
    {"D", "dh"}, // IPA ð
    {"s", "s"},
    {"z", "z"},
    {"x", ""}, //???
    {"G", ""}, // IPA weird y
    {"l", "l"}, // ????
    {"L", "el"}, // ???IPA lambda
    {"e", "eh"},
    {"r", "dx"}, // IPA ɾ
    {"i", "ty"},
    {"j", "y"},
    {"a", "aa"}, // ?
    {"o", "ao"}, // ?
    {"u", "uw"},
    {"w", "w"},
    {}
};

char *sampa_to_arpabet(char *sampa)
{
    int i;
    bool stress;
    char *pos;
    int size = sizeof(char) * 256;
    char *ret = malloc(size);
    char *ret_pos = ret;
    
    for (pos = sampa; pos < sampa + strlen(sampa);)
    {
        if (ret_pos - ret > 250)
        {
            size += sizeof(char) * 256;
            ret = realloc(ret, size);
        }
        if (*pos == '\'')
        {
            stress = true;
            pos += 1;
            continue;
        }
        for (i = 0; convert_table[i].sampa; i++)
        {
            if (!strncmp(pos, convert_table[i].sampa, strlen(convert_table[i].sampa)))
            {
                strcpy(ret_pos, convert_table[i].arpabet);
                ret_pos += strlen(convert_table[i].arpabet);
                if (stress)
                {
                    *ret_pos = '1';
                    ret_pos += 1;
                }
                *ret_pos = ' ';
                ret_pos += 1;
                stress = false;
                break;
            }
        }
        if (convert_table[i].sampa != NULL)
            pos += strlen(convert_table[i].sampa);
        else
            pos += 1;
    }
    printf("ret: %s\n", ret);
    return ret;
}


cst_utterance *es_lexical_insertion_saga(cst_utterance *u)
{
    cst_item *word;
    cst_relation *sylstructure, *seg, *syl;
    cst_lexicon *lex;
    const cst_val *lex_addenda = NULL;
    const cst_val *p, *wp = NULL;
    char *phone_name;
    const char *stress = "0";
    const char *pos;
    cst_val *phones;
    cst_item *ssword, *sssyl, *segitem, *sylitem, *seg_in_syl;
    const cst_val *vpn;
    int dp = 0;
    char *sampa = NULL, *arpa = NULL;

    SagaEngine *engine = SagaEngine_NewFromVariant("castilla");
    if (engine == NULL)
        printf("COULDNT INIT SAGA\n");

    lex = val_lexicon(feat_val(u->features, "lexicon"));
    if (lex->lex_addenda)
        lex_addenda = lex->lex_addenda;

    syl = utt_relation_create(u, "Syllable");
    sylstructure = utt_relation_create(u, "SylStructure");
    seg = utt_relation_create(u, "Segment");

    for (word = relation_head(utt_relation(u, "Word"));
         word; word = item_next(word))
    {
        ssword = relation_append(sylstructure, word);
        pos = ffeature_string(word, "pos");
        phones = NULL;
        wp = NULL;
        dp = 0;                 /* should the phones get deleted or not */

        /*        printf("awb_debug word %s pos %s gpos %s\n",
           item_feat_string(word,"name"),
           pos,
           ffeature_string(word,"gpos")); */

        /* FIXME: need to make sure that textanalysis won't split
           tokens with explicit pronunciation (or that it will
           propagate such to words, then we can remove the path here) */
        if (item_feat_present(item_parent(item_as(word, "Token")), "phones"))
        {
            vpn = item_feat(item_parent(item_as(word, "Token")), "phones");
            if (cst_val_consp(vpn))
            {                   /* for SAPI ?? */
                /* awb oct11: this seems wrong -- */
                /* not sure SAPI still (ever) works Oct11 */
                phones = (cst_val *) vpn;
            }
            else
            {
                dp = 1;
                if (cst_streq(val_string(vpn),
                              ffeature_string(word,
                                              "p.R:Token.parent.phones")))
                    phones = NULL;      /* Already given these phones */
                else
                    phones = val_readlist_string(val_string(vpn));
            }
        }
        else
        {
            wp = val_assoc_string(item_feat_string(word, "name"),
                                  lex_addenda);
            if (wp)
                phones = (cst_val *) val_cdr(val_cdr(wp));
            else
            {
                dp = 1;
                phones = lex_lookup(lex, item_feat_string(word, "name"), pos,
                                    u->features);
            }
        }
        printf("word: %s\n", item_feat_string(word, "name"));
        if (SagaEngine_TranscribeText(engine, item_feat_string(word, "name"), "ISO-8859-15",
                    &sampa, NULL, NULL, NULL, NULL) < 0) {
            printf("Error transcribing");
        }
        printf("sampa: %s\n", sampa);
        arpa = sampa_to_arpabet(sampa);
        phones = val_readlist_string(arpa);
        free(arpa);
        free(sampa);
        sampa = NULL;

        for (sssyl = NULL, sylitem = NULL, p = phones; p; p = val_cdr(p))
        {
            if (sylitem == NULL)
            {
                sylitem = relation_append(syl, NULL);
                sssyl = item_add_daughter(ssword, sylitem);
                stress = "0";
            }
            segitem = relation_append(seg, NULL);
            phone_name = cst_strdup(val_string(val_car(p)));
            if (phone_name[cst_strlen(phone_name) - 1] == '1')
            {
                stress = "1";
                phone_name[cst_strlen(phone_name) - 1] = '\0';
            }
            else if (phone_name[cst_strlen(phone_name) - 1] == '0')
            {
                stress = "0";
                phone_name[cst_strlen(phone_name) - 1] = '\0';
            }
            item_set_string(segitem, "name", phone_name);
            seg_in_syl = item_add_daughter(sssyl, segitem);
#if 0
            printf("awb_debug ph %s\n", phone_name);
#endif
            if ((lex->syl_boundary) (seg_in_syl, val_cdr(p)))
            {
#if 0
                printf("awb_debug SYL\n");
#endif
                sylitem = NULL;
                if (sssyl)
                    item_set_string(sssyl, "stress", stress);
            }
            cst_free(phone_name);
        }
        if (dp)
        {
            delete_val(phones);
            phones = NULL;
        }
    }

    return u;
}


void es_init(cst_voice *v)
{
    /* Basic generic functions that need to be registered always */
    basic_ff_register(v->ffunctions);

    /* 1. Tokenizer */
    es_init_tokenizer(v);

    /* 2. Utterance break function */
    feat_set(v->features,"utt_break",breakfunc_val(&default_utt_break));

    /* 3. Text analyser */
    feat_set(v->features,"tokentowords_func",itemfunc_val(&es_tokentowords));

    /* 4. very simple POS tagger */
    /* TO DO */

    /* 5. Phrasing */
    /* TO DO */

    /* 6a. Phoneset */
    /* TO DO */

    /* 7 Something something */
    feat_set(v->features, "lexical_insertion_func",
            uttfunc_val(es_lexical_insertion_saga));
    /* 8. Intonation */
    /* TO DO */

    /* 10. Duration */
    /* TO DO? */

    /* 11. f0 model */
    /* TO DO? */

}

/* Copyright (C) 2007-2016 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Victor Julien <victor@inliniac.net>
 *
 * Implements metadata keyword support
 *
 * \todo Do we need to do anything more this is used in snort host attribute table
 *       It is also used for rule managment.
 */

#include "suricata-common.h"
#include "detect.h"
#include "detect-parse.h"
#include "detect-metadata.h"

#define PARSE_REGEX "^\\s*([^\\s]+)\\s+([^\\s]+)(?:,\\s*([^\\s]+)\\s+([^\\s]+))*$"

static pcre *parse_regex;
static pcre_extra *parse_regex_study;

static int DetectMetadataSetup (DetectEngineCtx *, Signature *, char *);
static int DetectMetadataMatch(ThreadVars *t, DetectEngineThreadCtx *det_ctx,
                          Packet *p, Signature *s, const SigMatchCtx *ctx);

void DetectMetadataRegister (void)
{
    sigmatch_table[DETECT_METADATA].name = "metadata";
    sigmatch_table[DETECT_METADATA].desc = "partially used by suricata";
    sigmatch_table[DETECT_METADATA].url = DOC_URL DOC_VERSION "/rules/meta.html#metadata";
    sigmatch_table[DETECT_METADATA].Match = DetectMetadataMatch;
    sigmatch_table[DETECT_METADATA].Setup = DetectMetadataSetup;
    sigmatch_table[DETECT_METADATA].Free  = NULL;
    sigmatch_table[DETECT_METADATA].RegisterTests = NULL;

    DetectSetupParseRegexes(PARSE_REGEX, &parse_regex, &parse_regex_study);
}


static int DetectMetadataMatch(ThreadVars *t, DetectEngineThreadCtx *det_ctx,
                          Packet *p, Signature *s, const SigMatchCtx *ctx)
{
    const DetectMetadataData *ddata = (DetectMetadataData *)ctx;

    det_ctx->alert_flags |= ddata->flags;

    return 1;
}

static void* DetectMetadataParse(char *rawstr)
{
    int i;
    DetectMetadataData *de = NULL;
#define MAX_SUBSTRINGS 30
    int ret = 0, res = 0;
    int ov[MAX_SUBSTRINGS];

    ret = pcre_exec(parse_regex, parse_regex_study, rawstr, strlen(rawstr), 0, 0, ov, MAX_SUBSTRINGS);
    if (ret < 2) {
        SCLogError(SC_ERR_PCRE_MATCH, "pcre_exec parse error, ret %" PRId32
                ", string %s", ret, rawstr);
        goto error;
    }

    /* now check key value */
    for (i = 0; i < ret; i+=2) {
        const char *key;
        const char *value;

        res =  pcre_get_substring(rawstr, ov, MAX_SUBSTRINGS, i + 1, &key);
        if (res < 0) {
            SCLogError(SC_ERR_PCRE_GET_SUBSTRING, "pcre_get_substring failed");
            goto error;
        }

        res =  pcre_get_substring(rawstr, ov, MAX_SUBSTRINGS, i + 2, &value);
        if (res < 0) {
            SCLogError(SC_ERR_PCRE_GET_SUBSTRING, "pcre_get_substring failed");
            goto error;
        }

        if (!strncmp(key, "target", 7)) {
            if (!strncmp(value, "client", 7)) {
                de = SCMalloc(sizeof(DetectMetadataData));
                if (unlikely(de == NULL))
                    goto error;
                de->flags = PACKET_ALERT_SRC_IS_TARGET;
                break;
            } else if (!strncmp(value, "server", 7)) {
                de = SCMalloc(sizeof(DetectMetadataData));
                if (unlikely(de == NULL))
                    goto error;
                de->flags = PACKET_ALERT_DEST_IS_TARGET;
                break;
            } else {
                SCLogError(SC_ERR_INVALID_VALUE, "only src and dest are support for target metadata");
                goto error;
            }
        }
    }
    return de;

error:
    return NULL;
}

static int DetectMetadataSetup(DetectEngineCtx *de_ctx, Signature *s, char *rawstr)
{
    SigMatch *sm = NULL;

    /* parse the option, if NULL we are not doing anything */
    DetectMetadataData *ddata = DetectMetadataParse(rawstr);
    if (ddata == NULL)
        return 0;

    /* if we have something to do with the metadata then create the sm */
    sm = SigMatchAlloc();
    if (sm == NULL)
        return -1;

    sm->type = DETECT_METADATA;
    sm->ctx = (SigMatchCtx *) ddata;

    SigMatchAppendSMToList(s, sm, DETECT_SM_LIST_POSTMATCH);
    return 0;
}


/*
 * controller.c
 */

#include "utils.h"
#include "controller.h"

/**
 * Initializes the actions handler
 */
static struct actions_handler *_init_actions_handler(struct json_value_t *json_root)
{
    struct actions_handler *actions = (struct actions_handler *)malloc(sizeof(struct actions_handler));
    if (!actions) {
        duderr("Out of memory");
        return NULL;
    }

    actions->json_value = json_object_get_value(json_object(json_root), "actions");
    if (!actions->json_value) {
        duderr("Failed to retrieve \"actions\" JSON value");
        return NULL;
    }

    actions->num_actions = json_object_get_count(json_object(actions->json_value));
    if (!actions->num_actions) {
        duderr("Failed to retrieve number of actions \"num-actions\"");
        return NULL;
    }

    actions->idx = 0;

    return actions;
}

/**
 * Parse file output parameters and set _output_params to use them
 */
static struct output_handler *_set_output_params(struct json_value_t *json_output_value)
{
    struct output_handler *output = NULL;
    const char *directory_path = NULL;
    const char *name_suffix = NULL;
    struct output_params *fout_params = NULL;

    directory_path = json_object_get_string(
            json_object(json_output_value), "directory-path");
    if (!directory_path) {
        duderr("Export directory path not supplied: \"directory-path\"");
        goto fail;
    }

    name_suffix = json_object_get_string(
            json_object(json_output_value), "name-suffix");
    if (!name_suffix) {
        duderr("Name suffix for exported files not supplied: \"name-suffix\"");
        goto fail;
    }

    fout_params = (struct output_params *)malloc(sizeof(struct output_params));
    if (!fout_params) {
        duderr("Out of memory");
        goto fail;
    }

    fout_params->directory_path = directory_path;
    fout_params->name_suffix = name_suffix;

    output = (struct output_handler *)malloc(sizeof(struct output_handler));
    if (!output) {
        duderr("Out of memory");
        goto fail;
    }

    output->method = OUTPUT_FILEOUT;
    output->json_value = json_output_value;
    output->params = fout_params;

    return output;
fail:
    duderr("Out of memory");
    if (fout_params) {
        free(fout_params);
    }

    if (output) {
        free(output);
        output = NULL;
    }

    return NULL;
}

/**
 * Parse output parameters from JSON file and set parameters
 */
static struct output_handler *_init_output_handler(struct json_value_t *json_root)
{
    struct output_handler *output = NULL;
    struct json_value_t *json_output_value = NULL;
    const char *method = NULL;

    json_output_value = json_object_get_value(json_object(json_root), "output");
    if (!json_output_value) {
        duderr("failed to parse JSON output value");
        return NULL;
    }

    method = json_object_get_string(json_object(json_output_value), "method");
    if (!method) {
        duderr("output method was not specified");
        return NULL;
    }

    if (!strcmp(method, "file-out")) {
        output = _set_output_params(json_output_value);
    } else {
        duderr("unsupported export method: %s", method);
        return NULL;
    }

    return output;
}

/**
 * Iterate actions and construct data model
 */
int dudley_iterate_actions(dud_t *ctx)
{
    struct json_value_t *action_json_value = NULL;

    for (ctx->actions->idx = 0; ctx->actions->idx < ctx->actions->num_actions; ctx->actions->idx++) {
        action_json_value = json_object_get_value_at(
                json_object(ctx->actions->json_value), ctx->actions->idx);
        if (!action_json_value) {
            duderr("Failed to retrieve next JSON action (idx: %lu", ctx->actions->idx);
            return FAILURE;
        }

        //TODO: add logic to handle actions
        duderr("Not implemented, yet");
    }

    return SUCCESS;
}

/**
 * Cleanup dudley context
 */
void dudley_destroy(dud_t *ctx)
{
    if (ctx->actions) {
        free(ctx->actions);
    }

    if (ctx->output) {
        if (ctx->output->params) {
            free(ctx->output->params);
        }

        free(ctx->output);
    }

    if (ctx->json_root) {
        json_value_free(ctx->json_root);
    }

    free(ctx);
    ctx = NULL;
}

/**
 * Parse dudley JSON file and validate the schema
 */
dud_t *dudley_load_file(const char *filepath)
{
    const char *name = NULL;
    struct json_value_t *json_root = NULL;
    struct output_handler *output = NULL;
    struct actions_handler *actions = NULL;
    dud_t *ctx = NULL;
    struct json_value_t *schema = json_parse_string(
        "{"
            "\"name\":\"\","
            "\"output\": {},"
            "\"actions\": {}"
        "}"
    );

    json_root = json_parse_file(filepath);
    if (!json_root) {
        duderr("JSON formatted input is invalid");
        goto done;
    }

    if (json_validate(schema, json_root) != JSONSuccess) {
        duderr("Erroneous JSON schema");
        json_value_free(json_root);
        goto done;
    }

    name = json_object_get_string(json_object(json_root), "name");
    dudinfo("%s (%s) loaded successfully!", filepath, name);

    output = _init_output_handler(json_root);
    if (!output) {
        duderr("Failed to parse the output parameters");
        goto done;
    }

    actions = _init_actions_handler(json_root);
    if (!actions) {
        duderr("Failed to initialize the actions handler");
        goto done;
    }

    ctx = (dud_t *)malloc(sizeof(dud_t));
    if (!ctx) {
        duderr("Out of memory");
        goto done;
    }

    ctx->name = name;
    ctx->json_root = json_root;
    ctx->actions = actions;
    ctx->output = output;

done:
    json_value_free(schema);
    return ctx;
}

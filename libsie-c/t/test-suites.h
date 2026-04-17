Suite *api_suite(void);
Suite *decoder_suite(void);
Suite *exception_suite(void);
Suite *file_suite(void);
Suite *functional_suite(void);
Suite *histogram_suite(void);
Suite *id_map_suite(void);
Suite *object_suite(void);
Suite *output_suite(void);
Suite *progress_suite(void);
Suite *regression_suite(void);
Suite *relation_suite(void);
Suite *sifter_suite(void);
Suite *spigot_suite(void);
Suite *stringtable_suite(void);
Suite *xml_suite(void);
Suite *xml_merge_suite(void);

static struct {
    char *name;
    Suite *(*fn)(void);
} suites[] = {
    { "api", api_suite },
    { "decoder", decoder_suite },
    { "exception", exception_suite },
    { "file", file_suite },
    { "functional", functional_suite },
    { "histogram", histogram_suite },
    { "id_map", id_map_suite },
    { "object", object_suite },
    { "output", output_suite },
    { "progress", progress_suite },
    { "regression", regression_suite },
    { "relation", relation_suite },
    { "sifter", sifter_suite },
    { "spigot", spigot_suite },
    { "stringtable", stringtable_suite },
    { "xml", xml_suite },
    { "xml_merge", xml_merge_suite },
};

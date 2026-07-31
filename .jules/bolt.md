# Bolt's Journal

## 2025-02-15 - [Optimize blacklist lookup in GTK menu module]
**Learning:** Checking if an application is blacklisted is called whenever a GTK module checks if it should run (e.g., inside `gtk_module_should_run()`). The original implementation used a linear O(N) array iteration with `g_strcmp0` for each of the blacklisted items. By sorting the static string blacklist array alphabetically, we can perform a highly efficient O(log N) binary search (`bsearch`). This requires 0 heap allocation or first-use GHashTable initialization overhead, keeping it extremely lightweight and fast.
**Action:** Always consider sorting static lookups and using binary search (`bsearch`) instead of complex dynamic structures (like hash tables) or O(N) linear iteration for small static arrays.

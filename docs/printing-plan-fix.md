# Print plan stability improvements

## Issue summary

Selecting **Print plan** could freeze the application and never generate the PDF. Closing the app showed a flood of `std::invalid_argument` exceptions from `std::stof` while reading camera and grid settings. Every invalid or empty numeric entry triggered an exception on each lookup, and the 2D render loop performs thousands of lookups while capturing the plan. The repeated exceptions overwhelmed the export path and blocked PDF generation.

## Fixes

- Added a safe `std::from_chars`-based parsing helper in `ConfigManager` so malformed numeric values are trimmed, parsed without exceptions, and replaced with defaults when invalid.
- Hardened the PDF exporter with checks for empty buffers, missing/invalid destinations, unusable margins, and non-finite viewport data, reporting descriptive errors instead of failing silently.
- Updated the **Print plan** action to trim the chosen path, keep the UI responsive by exporting off the main thread, and show the exporter’s success or error message to the user.

## Result

Plan printing completes without flooding the log or stalling. Invalid configuration entries are ignored safely, and users receive clear feedback when an export problem occurs.

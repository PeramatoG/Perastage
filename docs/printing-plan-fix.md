# Print plan stability improvements

## Issue summary

Selecting **Print plan** could freeze the application for minutes and never generate the PDF. Closing the app revealed a flood of `std::invalid_argument` exceptions coming from the C++ runtime. The 2D print flow pulled camera/grid values from the persisted configuration using `std::stof`, so any user-edited or empty entries triggered an exception on every lookup. The rendering loop hits those lookups thousands of times while capturing the plan, so the repeated exceptions caused the apparent hang and prevented the PDF from being written.

## Fixes

- Replaced exception-driven numeric parsing in `ConfigManager` with a safe, non-throwing `std::from_chars` helper. Invalid or blank values now fall back to defaults without generating exceptions.
- Hardened the PDF exporter with validation for viewport data, paper size, and output paths, returning detailed error messages instead of failing silently.
- Updated the **Print plan** action to validate the destination file and surface exporter errors back to the user.

## Result

Printing the plan now completes without flooding the log with `std::invalid_argument` exceptions. Invalid configuration entries are ignored safely, and users receive clear feedback if an export problem occurs.

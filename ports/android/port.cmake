# Android is planned, not built: the launcher cannot exec a separate engine
# process there, and Surreal Engine has no Android support yet.
message(FATAL_ERROR "The android port is not implemented yet -- see ports/android/README.md")

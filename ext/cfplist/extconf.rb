# frozen_string_literal: true

require "mkmf"
require "rbconfig"

# Force clang compilation
RbConfig::CONFIG["SDKROOT"] = %x(xcrun --sdk macosx --show-sdk-path)
RbConfig::MAKEFILE_CONFIG["CC"] = "clang"
RbConfig::MAKEFILE_CONFIG["CXX"] = "clang++"

def ensure_framework(framework)
  have_framework(framework) || raise("#{framework} framework not found!")
  have_header("#{framework}/#{framework}.h") || raise("Could not find #{framework}.h!")
end

def ensure_frameworks(*frameworks)
  frameworks.each do |framework|
    ensure_framework(framework)
  end
end

ensure_framework "CoreFoundation"

dir_config "cfplist"

create_makefile("cfplist/cfplist")

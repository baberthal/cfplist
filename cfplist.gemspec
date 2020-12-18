# frozen_string_literal: true

require_relative "lib/cfplist/version"

Gem::Specification.new do |spec|
  spec.name          = "cfplist"
  spec.version       = CFPlist::VERSION
  spec.authors       = ["J. Morgan Lieberthal"]
  spec.email         = ["j.morgan.lieberthal@gmail.com"]

  spec.summary       = "CoreFoundation PropertyList Native Bindings"
  spec.description   = "Native bindings for CoreFoundation PropertyList " \
    "files. Note that this gem requires the CoreFoundation framework " \
    "be present on the system, so it will only work on macOS."
  spec.homepage      = "https://github.com/baberthal/cfplist"
  spec.license       = "MIT"
  spec.required_ruby_version = Gem::Requirement.new(">= 2.6.0")

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = spec.homepage
  spec.metadata["changelog_uri"] = "https://github.com/baberthal/cfplist/blob/prime/CHANGELOG"

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files = Dir.chdir(File.expand_path(__dir__)) do
    %x(git ls-files -z).split("\x0").reject { |f| f.match(%r{^(test|spec|features)/}) }
  end
  spec.bindir        = "exe"
  spec.executables   = spec.files.grep(%r{^exe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/cfplist/extconf.rb"]
end

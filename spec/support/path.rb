# frozen_string_literal: true

require "pathname"

module Spec
  module Path
    def self.included(base)
      base.extend Spec::Path
    end

    def root
      @root ||= Pathname.new(File.expand_path("../..", __dir__))
    end

    def spec_dir
      @spec_dir ||= Pathname.new(File.expand_path(root.join("spec"), __FILE__))
    end

    def tmp(*path)
      root.join("tmp", *path)
    end

    def fixtures_dir(*path)
      spec_dir.join("fixtures", *path)
    end

    alias fixtures fixtures_dir

    extend self # rubocop:disable Style/ModuleFunction
  end
end

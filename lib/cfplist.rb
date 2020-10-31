# frozen_string_literal: true

require "cfplist/version"
require "cfplist/cfplist"

module CFPlist
  class Error < StandardError; end

  def self.[](object, opts = {})
    if object.respond_to? :to_str
      CFPlist.parse(object.to_str, opts)
    else
      CFPlist.generate(object, opts)
    end
  end

module_function

  def parse(data, opts = {})
    symbolize_keys = opts.fetch(:symbolize_keys, false)
    _parse(data, symbolize_keys)
  end

  def generate(obj, opts = {})
    _generate(obj, opts)
  end
end

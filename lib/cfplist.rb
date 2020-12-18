# frozen_string_literal: true

require "cfplist/version"
require "cfplist/cfplist"

# Main CFPlist Module.
module CFPlist
  class << self
    def [](object, opts = {})
      if object.respond_to? :to_str
        CFPlist.parse(object.to_str, opts)
      else
        CFPlist.generate(object, opts)
      end
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

  class << self
    # Default options for {#load}.
    # Initially:
    #   opts = CFPlist.load_default_options
    #   opts # => {:symbolize_keys => false}
    # @return [Hash{Symbol => Boolean}]
    attr_accessor :load_default_options
  end
  self.load_default_options = {
    symbolize_keys: false
  }

  def load(source, proc = nil, options = {})
    opts = load_default_options.merge(options)
    if source.respond_to?(:to_str)
      source = source.to_str
    elsif source.respond_to?(:to_io)
      source = source.to_io.read
    elsif source.respond_to?(:read)
      source = source.read
    end

    result = parse(source, opts)
    recurse_proc(result, &proc) if proc
    result
  end

  # Recursively calls passed _Proc_ if the parsed data structure is an _Array_
  # or a _Hash_.
  def recurse_proc(result, &proc) # :nodoc:
    case result
    when Array
      result.each { |x| recurse_proc(x, &proc) }
      proc.call(result)
    when Hash
      result.each { |_k, _v| recurse_proc(x, &proc); recurse_proc(y, &proc) }
      proc.call(result)
    else
      proc.call(result)
    end
  end

  alias restore load
  module_function :restore

  class << self
    # Default options for {#parse}
    # Initially:
    #   opts = CFPlist.dump_default_options
    #   opts # => {}
    # @return [Hash{Symbol => Boolean}]
    attr_accessor :dump_default_options
  end
  self.dump_default_options = {}

  def dump(object, an_io = nil, limit = nil)
    if an_io && limit.nil?
      an_io = an_io.to_io if an_io.respond_to?(:to_io)
      unless an_io.respond_to?(:write)
        limit = an_io
        an_io = nil
      end
    end

    opts = CFPlist.dump_default_options
    opts = opts.merge(max_nesting: limit) if limit
    result = generate(object, opts)

    if an_io
      an_io.write(result)
      an_io
    else
      result
    end
  end
end

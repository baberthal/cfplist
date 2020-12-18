# CFPlist

Native bindings for CoreFoundation Property List files in ruby.

Note this this gem will only work on platforms where the
`CoreFoundation.framework` is present, which means macOS.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'cfplist'
```

And then execute:

    $ bundle install

Or install it yourself as:

    $ gem install cfplist

## Usage

To load a property list from a string, do this:

```ruby
require "cfplist"

data = File.read("/path/to/whatever.plist")
plist = CFPlist.parse(data)

```
This will return either an `Array` or a `Hash`, depending on the structure of
the property list.


To generate a property list from an an `Array` or `Hash`, do this:

```ruby
require "cfplist"

my_hash = { "foo" => "bar", "baz" => "quux" }
data = CFPlist.generate(my_hash)  # => data is a string containing the generated plist

File.open("/path/to/whatever.plist", "r") do |io|
  io.write(data)
end

```


The following methods are also implemented for compatibility with the `json` gem
and the `Marshal` API:

* `.[](object, opts = {})`
  - If _object_ is string-like, parse the string and return the parsed result as
    a ruby data structure. Otherwise, generate Property List text from the Ruby
    data structure and return it.

* `.dump(object, an_io = nil, limit = nil)`
  - Dumps _obj_ as a Property List string, i.e. calls `.generate` on the object
    and returns the result.
  - If `an_io` (an IO-like object or an object that responds to the #write)
    method was given, the resulting Property List is written to it.
  - If the number of nested arrays or objects exceeds limit, an ArgumentError
    exception is raised. This argument is similar (but not exactly the same!)
    to the limit argument in Marshal.dump.

* `.load(source, proc = nil, options = {})`
  - Load a ruby data structure from a Property List source and return it.
    A source can either be a string-like object, an IO-like object, or an object
    responding to the read method. If proc was given, it will be called with any
    nested Ruby object as an argument recursively in depth first order.
  - BEWARE: This method is meant to serialise data from trusted user input,
    like from your own database server or clients under your control, it could
    be dangerous to allow untrusted users to pass JSON sources into it.

## Development

After checking out the repo, run `bin/setup` to install dependencies. Then, run `rake spec` to run the tests. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and tags, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/baberthal/cfplist. This project is intended to be a safe, welcoming space for collaboration, and contributors are expected to adhere to the [code of conduct](https://github.com/baberthal/cfplist/blob/prime/CODE_OF_CONDUCT.md).


## License

The gem is available as open source under the terms of the [MIT License](https://opensource.org/licenses/MIT).

## Code of Conduct

Everyone interacting in the Cfplist project's codebases, issue trackers, chat rooms and mailing lists is expected to follow the [code of conduct](https://github.com/baberthal/cfplist/blob/prime/CODE_OF_CONDUCT.md).

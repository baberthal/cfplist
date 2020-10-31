# frozen_string_literal: true

RSpec.describe CFPlist do
  let(:dict_data) { fixtures("example-dict.plist").read }
  let(:array_data) { fixtures("example-array.plist").read }

  it "has a version number" do
    expect(CFPlist::VERSION).not_to be nil
  end

  describe ".parse" do
    it "properly parses the plist dict without raising an error" do
      expect { described_class.parse(dict_data) }.not_to raise_error
    end

    it "properly parses the plist array without raising an error" do
      expect { described_class.parse(array_data) }.not_to raise_error
    end

    it "properly parses the plist dictionary" do
      plist = described_class.parse(dict_data)
      expect(plist["FirstName"]).to eq "John"
    end

    it "properly parses the plist array" do
      plist = described_class.parse(array_data)
      expect(plist[0]).to eq 1
    end

    it "symbolizes the keys if :symbolize_keys is true" do
      plist = described_class.parse(dict_data, symbolize_keys: true)
      expect(plist[:FirstName]).to eq "John"
    end
  end

  describe ".load" do
    pending "Not implemented"
  end

  describe ".generate" do
    context "when passed an array" do
      let(:array) { [1, "two", { "c" => 13 }] }

      it "generates a plist from the array" do
        expect(described_class.generate(array)).to \
          match(%r{<array>\s+<integer>1</integer>\s+<string>two</string>\s+
                  <dict>\s+<key>c</key>\s+<integer>13</integer>\s+</dict>\s+
                  </array>}x)
      end
    end

    context "when passed a hash" do
      let(:hash) do
        {
          "FirstName" => "John",
          "LastName" => "Public",
          "StreetAddr1" => "123 Anywhere St.",
          "StateProv" => "CA",
          "City" => "Some Town",
          "CountryName" => "United States",
          "AreaCode" => "555",
          "LocalPhoneNumber" => "5551212",
          "ZipPostal" => "12345"
        }
      end

      it "generates a plist from the hash" do
        expect(described_class.generate(hash)).to \
          match %r{<key>AreaCode</key>\s+<string>555</string>}
      end
    end
  end

  describe ".dump" do
    pending "Not implemented"
  end

  describe ".[]" do
    before do
      allow(described_class).to receive(:parse).and_call_original
      allow(described_class).to receive(:generate).and_call_original
    end

    context "when passed a string-like object" do
      let(:plist) { described_class[array_data] }

      it "parses the string" do
        _ = plist # to make sure we call the method
        expect(described_class).to have_received(:parse).with(array_data, {})
      end
    end

    context "when passed a hash-like object" do
      let(:data) { { name: "Morgan" } }

      it "generates a plist from the data" do
        _result = described_class[data]
        expect(described_class).to have_received(:generate).with(data, {})
      end
    end
  end
end

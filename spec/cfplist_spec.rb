# frozen_string_literal: true

RSpec.describe CFPlist do
  it "has a version number" do
    expect(CFPlist::VERSION).not_to be nil
  end

  describe ".parse" do
    let(:data) { fixtures("example.plist").read }

    it "properly parses the plist without raising an error" do
      expect { described_class.parse(data) }.not_to raise_error
    end

    it "properly parses the plist" do
      plist = described_class.parse(data)
      expect(plist["FirstName"]).to eq "John"
    end

    it "symbolizes the keys if :symbolize_keys is true" do
      plist = described_class.parse(data, symbolize_keys: true)
      expect(plist[:FirstName]).to eq "John"
    end
  end
end

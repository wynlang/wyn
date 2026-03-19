class Wyn < Formula
  desc "Fast, simple programming language that compiles to C"
  homepage "https://wynlang.com"
  url "https://github.com/user/wyn-lang/archive/refs/tags/v1.8.0.tar.gz"
  sha256 "PLACEHOLDER"
  license "MIT"

  depends_on "make" => :build

  def install
    cd "wyn" do
      system "make"
      bin.install "wyn"
      # Install runtime files needed for compilation
      (share/"wyn/src").install Dir["src/*.h"]
      (share/"wyn/src").install Dir["src/*.c"]
      (share/"wyn/vendor/minicoro").install "vendor/minicoro/minicoro.h"
      (share/"wyn/vendor/tcc/lib").install Dir["vendor/tcc/lib/*"]
      (share/"wyn/vendor/tcc/bin").install "vendor/tcc/bin/tcc" if File.exist?("vendor/tcc/bin/tcc")
      (share/"wyn/vendor/tcc/tcc_include").install Dir["vendor/tcc/tcc_include/*"]
      (share/"wyn/runtime").install Dir["runtime/*.a"] if Dir.exist?("runtime")
    end
  end

  test do
    (testpath/"hello.wyn").write 'println("Hello from Wyn!")'
    assert_match "Hello from Wyn!", shell_output("#{bin}/wyn run hello.wyn 2>&1")
  end
end

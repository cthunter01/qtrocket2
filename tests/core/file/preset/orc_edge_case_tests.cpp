// The edge cases of the probe tools/openrocket-probes/orc-loader (OrcEdgeProbe, its expected.txt)
// replayed through OrcDocument, OrcElement and JaxbLexical: for every case, what QtRocket's reading
// of the document gives at this level (whether it parses, which elements and attributes count,
// their texts and the values JAXB reads from them), pinned by hand from what OpenRocket's loader
// made of it. The values in the comments are the probe's, in SI units after OpenRocket's unit
// conversion (an inch is 0.0254 m); the tests pin the numbers as they are written in the document,
// before that conversion, which is OpenRocketComponentLoader's. What the loader then does (the
// units, the materials, the presets and the failures that are Java exceptions: "THROWS ...") is
// pinned with it.

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/preset/JaxbLexical.h"
#include "QtRocket/file/preset/OrcDocument.h"
#include "QtRocket/file/preset/OrcElement.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Sha256.h"
#include "file/preset/OrcEdgeCases.h"

namespace
{

using QtRocket::OrcDocument;
using QtRocket::OrcElement;
using QtRocket::Result;
using QtRocket::Test::orcEdgeCaseBytes;
using QtRocket::Test::orcEdgeCases;

constexpr double kInf = std::numeric_limits<double>::infinity();

/// ShapeDTO's constants, in its order.
constexpr std::array<std::string_view, 6> kShapes{"CONICAL", "OGIVE",     "ELLIPSOID",
                                                  "POWER",   "PARABOLIC", "HAACK"};
constexpr std::size_t                     kOgive = 1;
constexpr std::size_t                     kPower = 3;
constexpr std::size_t                     kHaack = 5;

/// MaterialTypeDTO's constants, in its order.
constexpr std::array<std::string_view, 3> kMaterialTypes{"LINE", "SURFACE", "BULK"};

/// The size and SHA-256 of a case's document, as the probe printed them ("document <size> bytes,
/// sha256 <digest>").
struct PinnedDocument
{
    std::string_view id;
    std::size_t      size;
    std::string_view sha256;
};

// clang-format off
constexpr auto kPinnedDocuments = std::to_array<PinnedDocument>({
    {.id = "01", .size = 664, .sha256 = "70a5f89fda7646349bebde4fb8d1a6cc620a97360bb654d70977f987f1d1da43"},
    {.id = "02", .size = 682, .sha256 = "c14dfbd8cb66d5365ab720d83406bd3274982d95a3f10bcc5ef372d8fe7742d2"},
    {.id = "03", .size = 715, .sha256 = "a8a18fe216a8df4fed70c95deb14d0ce37c40dc0489f7305af66b6d182d422e2"},
    {.id = "04", .size = 633, .sha256 = "edfb7b2728ab8961c2e14053cc34a53fd4a63ab2a6971f6954388e56ebbcba27"},
    {.id = "05", .size = 560, .sha256 = "70099c09cd2179b8847fa239927be9b3c80aebc24ffca41836117bbb6df1b9e7"},
    {.id = "06", .size = 565, .sha256 = "48adbd2a8c298b04f532d2cc001d3a65d5cfcb2abc58d28532e6fbe6c92fa488"},
    {.id = "07", .size = 663, .sha256 = "0f735a99dc9c8f88686767d3384d71f3d29ba4e15778f333a71e1429c69603ea"},
    {.id = "08", .size = 660, .sha256 = "f23f028a4d6d36d46309ef3e56557c466d37f4189ace2b29e553a83e8e65c2d6"},
    {.id = "09", .size = 668, .sha256 = "e6136208ff22179940766afe98a32d709ac9ca0a495a1b4c2219b44be3e99b71"},
    {.id = "10", .size = 665, .sha256 = "b88464a5de300f2f266c854aeb969c0812765de623b4815ddf9a0fef305c0524"},
    {.id = "11", .size = 663, .sha256 = "6395d4b93e7df5da73ad5edfb218e70ea855ad57f880d88bb095bd55977babfa"},
    {.id = "12", .size = 663, .sha256 = "1fdae6f4db92dfbd956b1d3908adf4f6b5be8c9065b7cd4471e56e1d33930413"},
    {.id = "13", .size = 663, .sha256 = "4ba5fbdaa0ccc6447b8531e7cb522a714a0101dfdbd9550e00bc3a487facfe2f"},
    {.id = "14", .size = 668, .sha256 = "989e4f934fbb0db30f1a80917b5b06ae4c4dd52258c53c40cc1e0fbe2244ca2a"},
    {.id = "15", .size = 663, .sha256 = "cf859eaf9c20c2ed798c7fb8733c87170e2099250f91bd7ecf14050c06861129"},
    {.id = "16", .size = 666, .sha256 = "baab082da71154806e68b462be75197e474f6a3abdd46bb2dff9fcc3f05aab99"},
    {.id = "17", .size = 664, .sha256 = "3c525785a1c663dcd631a14d6069a3a1e968159d9ee03c8182e2bf4891875ea9"},
    {.id = "18", .size = 662, .sha256 = "1d3a39460231992c2124693e3352a58d2c49f2c14533bd7a5648cdca943a1f58"},
    {.id = "19", .size = 662, .sha256 = "4838f3270c69ae41d79b33b44bcfcceaaa3861f750ef958f73a28b76fd9807ca"},
    {.id = "20", .size = 669, .sha256 = "2e8147f7b2bae77ab58c98b70422a91c81a9acbb7aba27c12dbe31d279e1d31b"},
    {.id = "21", .size = 654, .sha256 = "8b47e1336b04f1e00ba2c8c6bf1b8ae0c151158169ad720b9ab759677284686d"},
    {.id = "22", .size = 667, .sha256 = "2c5d472de45f608ae193ba79faa6fedcd24634c13fd83cdd5203f7c135407371"},
    {.id = "23", .size = 664, .sha256 = "1a05f9dba903e45fe3056a91cb5d795c0af432cc985b126890f43ef233ec9d20"},
    {.id = "24", .size = 665, .sha256 = "33fb3eda588b2c05ea6750c3a5314cb9a2f59948b208548a7fc1c5ccc257aeb2"},
    {.id = "25", .size = 695, .sha256 = "f269c11808aabcc681a63d9571bb2856b6ad6bdc9f981ce81f30c458c0fa32d1"},
    {.id = "26", .size = 666, .sha256 = "bf8e571e1d48f1133b23595f8ad73218a6166b2ac78cedee6321e6eb73a197f8"},
    {.id = "27", .size = 663, .sha256 = "77cb4734930ff2be26ca57a894e0abc45b6edf6ed8e6324dd1f0d7533b268b57"},
    {.id = "28", .size = 652, .sha256 = "2695bbc2abc36f0775c8fbf7a64e10aa3237f47f382c23eb97f4a8f6329e325a"},
    {.id = "29", .size = 665, .sha256 = "003251b656ebfa1c01f53fd0ff90418c7f0abe4c170f1bf6ee1874c884491591"},
    {.id = "30", .size = 666, .sha256 = "5b9defd1a777cc90b374e6e9edc77b6af75f173ea123009a0e3dd7a11ad2c731"},
    {.id = "31", .size = 622, .sha256 = "5a158e556de0df4592a9209603aa824d772e8fb87ac406271a3e0f256720679e"},
    {.id = "32", .size = 688, .sha256 = "28fdb82d55a99f35618e2c09f9c6e8d577adbb839c206b597819ad6ed0b11e50"},
    {.id = "33", .size = 692, .sha256 = "9af7736ae7fa6bc8288b0a84ebfa63a78acae15ca6f3e71b54ae7e4e881448b4"},
    {.id = "34", .size = 374, .sha256 = "8239b4f3e5cc1459af6a8bcd05294b718c50dd4f10cce1b894658f358f72068d"},
    {.id = "35", .size = 371, .sha256 = "745543c5ee116d2fca7a9a504031bdb8fb2212968cf9420f9201b5a9734df5cf"},
    {.id = "36", .size = 0, .sha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    {.id = "37", .size = 420, .sha256 = "d56b588b91cfe02e44b5890669d25ada2bd828926b2e1575a9d1727121e0f247"},
    {.id = "38", .size = 642, .sha256 = "c92df6c6462b2ad824e6e3677adcfed78d79dab40b3d42479fabe9520acb0451"},
    {.id = "39", .size = 664, .sha256 = "eb07fdef03b57464c647f0084371a73b3056cc9d6224f7fc20e573210eba6849"},
    {.id = "40", .size = 683, .sha256 = "25c436e2c7fbc85d62e152b2af2875c922d5ca1c1d1162713e8755c6b603952c"},
    {.id = "41", .size = 357, .sha256 = "a9f960b1d229176c4a706066f7087099ed59bd6501a1ef3b679266c63bc9f50a"},
    {.id = "42", .size = 381, .sha256 = "ff1b3c13cfb9a8eae689e5cacf43f5a7bd14b7dfb409205931e216a78713a3b9"},
    {.id = "43", .size = 713, .sha256 = "5a8232fc94a38e84d03a69deb7665dd08369b8a91ce2a93651cf82b7f8abd5a2"},
    {.id = "44", .size = 665, .sha256 = "da563dd66bdc8868a2d6fff7aad56cba92e21a121f621d445db5debf30470448"},
    {.id = "45", .size = 660, .sha256 = "77ed5a66d4061c8b745d8e6f758dc98462be68239bc5836ec1bda8ba2271c597"},
    {.id = "46", .size = 641, .sha256 = "5912fe81b799395cacc085da57b70bad9b473cec265295bfb314b7c4fa53563d"},
    {.id = "47", .size = 704, .sha256 = "eb08de93a469be23e7490aba2be4207faf65d9244b1f7b2690071dad883fa23a"},
    {.id = "48", .size = 704, .sha256 = "047dbb19e22e0ad7b7536d317e713fdaae5f343478cf01d085912dd686962bb8"},
    {.id = "49", .size = 664, .sha256 = "8972f4608fb2d3f168dae4fbd56895b83c37e05371065162288089deea919999"},
    {.id = "50", .size = 544, .sha256 = "41fd7b75571bb253edb754f3f0211a0436e40ab5519c0eb65340f6c247ed1709"},
    {.id = "51", .size = 542, .sha256 = "2ffcc1c9e0c13b64743805e4e7ae41e7f07b6ec1833b34eb32d0d1982042d843"},
    {.id = "52", .size = 545, .sha256 = "333fa96d64a4440f7996863306a12d96d667dc0ded6b0a30e8ac08830cc31c93"},
    {.id = "53", .size = 711, .sha256 = "d2865ea6b3d95318176432103a23b4bb9081c7aab17366e8bacd06db31c4d4ad"},
    {.id = "54", .size = 687, .sha256 = "2bc23f9f027183d7cb85b58ea611d99dae3579aa3f7ead9b39937ebf5467c56f"},
    {.id = "55", .size = 713, .sha256 = "23f030c32c18b9160c607207220771a87acef2f7ef14e0cad7b0c14f0c72a13c"},
    {.id = "56", .size = 713, .sha256 = "98f73cc4c1162302b8f2bb0ef6fb4e9ffafc82d3cec5504ae2abbd128c1c7609"},
    {.id = "57", .size = 674, .sha256 = "d066bd11779af1de357e3bccf20875877b3e5f86bcaa9292a00fa7ee3323a256"},
    {.id = "58", .size = 759, .sha256 = "d31159435ecfe4806de02f34ad347b646e5171eba9514ef5252f91f48fb3a207"},
    {.id = "59", .size = 707, .sha256 = "0cab81211fd7004708818c3d8d74b39b639da07613217840e696e7f5d954cb8a"},
    {.id = "60", .size = 704, .sha256 = "b86ade4a95b259e80ec00c6950c860b6da495bd8a01733135e0311e7aa4f2f6b"},
    {.id = "61", .size = 665, .sha256 = "8c2042d52f8d1632ce86fc523e331a44c048ddaeb89a4c9b163a8c136ffbea0e"},
    {.id = "62", .size = 665, .sha256 = "f429922f52b2a0bcc5a83d8510c2eea74be7997501e9818e1355ec98fc27cdd6"},
    {.id = "63", .size = 603, .sha256 = "674e0f846611122342c634e51c00d260c8421b33b96d3cbf9ee65f8208d1f062"},
    {.id = "64", .size = 736, .sha256 = "8adae79fcc573aa1941f3d10ea8089d169735eab7417ee39e624edb117d1c156"},
    {.id = "65", .size = 550, .sha256 = "ca01b4126b65ac98858e44d897888017fd668d1a68c3ce08a707fbf9d1bad9f9"},
    {.id = "66", .size = 726, .sha256 = "247b5af5911acd5824bceccc42f148a5efee75ba2d813b24c62a3c88dff5ce47"},
    {.id = "67", .size = 484, .sha256 = "8c39c55de796aed70d1c5be11006e1ba6e40b302f122b74872a8634519791d7d"},
    {.id = "68", .size = 636, .sha256 = "125210d4b0113da32718d519dcf2eb25a418b40f75ccdc02a07fe5fe370067ad"},
    {.id = "69", .size = 686, .sha256 = "e96d8d0172f64c0ca23cce6983ddbd53004b438d627d4156dbc7f1b3076f8bfe"},
    {.id = "70", .size = 682, .sha256 = "ad0440ba1285db4fcca4c8c80ec643020f4b71467c5541018ea17d7a306d5428"},
    {.id = "71", .size = 703, .sha256 = "a021073120e4a220c6bbd6aaaa4d7488f5e95c7199b1991e9abeb1fda82b0887"},
    {.id = "72", .size = 460, .sha256 = "42e717b41f114d9304d9445a0db02a77e69ad7526f5ffbc9a3c83920ab0742e5"},
    {.id = "73", .size = 668, .sha256 = "44cd79542e08ba6cb9ef346488db71bae06359d0faf6bd9a12723728d1b603e9"},
    {.id = "74", .size = 946, .sha256 = "12ec408a5a2cbe9d7f779f062a745f689744b84299723389eb769af79e2dbc42"},
    {.id = "75", .size = 695, .sha256 = "bdb8dd8cdf88e91494513524aee5e21a2217d2cc6d46a4051c62b5a81bb95dac"},
    {.id = "76", .size = 690, .sha256 = "9d97a692081107ace7d95aa72d06ceeccb357aef06a140c9ef1daf1d6baeaa14"},
    {.id = "77", .size = 676, .sha256 = "143667bd2df5feb822aa52f253947658b144a20978439cad274a83030152aefa"},
    {.id = "78", .size = 667, .sha256 = "79284746dca0aae58b24aede8ef5f09113dd65545245e115f033c425f325904f"},
    {.id = "79", .size = 665, .sha256 = "11f20a66948888aa0037741dbc165424bc54f0a684ff8f827abc6c9873858c21"},
    {.id = "80", .size = 665, .sha256 = "b8ac28499ec6cf729d664c1c777c4800f6260a15e267208e7e738afeb9acf11f"},
    {.id = "81", .size = 667, .sha256 = "7773b3302d18d5564f83b6cb0afa9d21b07a004e583dc56a675eb316b034676e"},
    {.id = "82", .size = 1332, .sha256 = "86a5a8cec7353f2b2d24873a36461b2458e8f88cf38a7cf28dea02a0da566721"},
    {.id = "N00", .size = 661, .sha256 = "e2085ac9317c04df2a29ba12069e879c411e2e91390690bbbb535b30c75fb616"},
    {.id = "N01", .size = 662, .sha256 = "d09a699a3c1a14347cc1d1c8d19fa73c7051e5ec49393b93670f3825abe02279"},
    {.id = "N02", .size = 664, .sha256 = "f8737bdb2432d452d6a977749658365d148f89be25852db7423b34ef3051ff88"},
    {.id = "N03", .size = 665, .sha256 = "8c22bc5e7f5b3fbb95eb6678a41d1738f529f97dfebf618dedd3b52737d0d324"},
    {.id = "N04", .size = 666, .sha256 = "5830055f3271fef9822d8b9dc8d38da7ac5797d1b13c6d50c25ac7e596f32241"},
    {.id = "N05", .size = 663, .sha256 = "d9e1b6fe3fba2b376db56c69b5c1519b98b5b16c5fc593b56b934651408ceb32"},
    {.id = "N06", .size = 662, .sha256 = "630a77fa96e933620c8c8c6640fc33eeaeca43c2c21944152fd8ae8921283f68"},
    {.id = "N07", .size = 662, .sha256 = "ebdbf2a0b91e77cc161574d9b871b52f3913fc5bea8fefa01e147b5b029bbcd4"},
    {.id = "N08", .size = 663, .sha256 = "ab2f33eb46eeb69e868b1489c270aa848e49f5a261f7ce79c21ebd97d7fd49fd"},
    {.id = "N09", .size = 662, .sha256 = "8e1167d1e1aa40604d4e159e59f777a73799746beb716a85c9f760d6366116a1"},
    {.id = "N10", .size = 663, .sha256 = "2fa4e9bb33608d8180ab4d30e6e30e72af80f22b760d4c4937616c4e230274b4"},
    {.id = "N11", .size = 663, .sha256 = "44dc6efdef5d952335a7744e5aec2b941c0a41ac273f2905f2368c187e1e9a2e"},
    {.id = "N12", .size = 661, .sha256 = "fb401c9c98d8b774ad4ede3456d69755e75ff2b433e82f02090e1847b1294d65"},
    {.id = "N13", .size = 661, .sha256 = "96c2c3ce5e767e6c128ee60c60d0e2f100f42e6381efd5270ee28b670a332fbb"},
    {.id = "N14", .size = 660, .sha256 = "64373942273ed9461e555e74a3466b0642e2d55f341c5ecb94513d8fdc4e4d64"},
    {.id = "N15", .size = 662, .sha256 = "f4097452c022ee6b84e6199719667b4c25c4a30e0964c241c2b6886788e07af1"},
    {.id = "N16", .size = 662, .sha256 = "c0750aed9c6cc487dcbdbe604a57ba19b9a67be5638cae5d8d5278934b1e313f"},
    {.id = "N17", .size = 663, .sha256 = "f0d9409d53b9d08a9640f78e699f4490337bff300db2bfb661cd9966b42a84b3"},
    {.id = "N18", .size = 664, .sha256 = "24ab5b9c4d7f3adf2e900b90db26ee63c68d0b57a9ff24c9c1c12e6f0f79f4c7"},
    {.id = "N19", .size = 663, .sha256 = "6876b145e851a0da56789e25ab157569d36e17b8773e3ba3051c99f46c99f778"},
    {.id = "N20", .size = 663, .sha256 = "19bf664014005e1992c27cf139cc3b8dfe80c1ccfd2df608bd9bb8fd331e7cc6"},
    {.id = "N21", .size = 662, .sha256 = "ff000d171b430c15085ab9d4b106a36fd8539f522956e461f4b428e8598cf771"},
    {.id = "N22", .size = 663, .sha256 = "472da3f829fba0a2d7ddae2b7a669c9023eac10af0c7c8d543524de4f2259f45"},
    {.id = "N23", .size = 663, .sha256 = "920e9b114c9e6488cafe64f0767893caba3712530dd128fb0bbf40075e72092b"},
    {.id = "N24", .size = 664, .sha256 = "84a4af34f4aab57ee22ab21e06c71abdc8968aad92b1436cd4da2acf5f8ed7db"},
    {.id = "N25", .size = 663, .sha256 = "f8a0b2199333f3aa9d08f80efe9a4a0dc20b9fff3061fd54e94a3312cd4598e2"},
    {.id = "N26", .size = 663, .sha256 = "6ef9c87a2b31ee201d7017739eb719bd10fbfcc572f79e1c9174ce47ee8e2a48"},
    {.id = "N27", .size = 665, .sha256 = "9bc973eccd7058ab4b85b0ff32120e54f8bbcc63ec58488a523c65c924d19fdc"},
    {.id = "N28", .size = 667, .sha256 = "aea895e7373cd086c75869c7761ab6c3233092b03679b0c2a5bd2e50762f0488"},
    {.id = "N29", .size = 682, .sha256 = "eec3f37b8af90eb62fa1393e56c133598a67324fa8be5826bed8a7bb8c4203dd"},
    {.id = "N30", .size = 675, .sha256 = "cfc314e6f6b71b456033c9aa7a5c18463f5767a367c0b2e417f25ec75c30f34c"},
    {.id = "N31", .size = 662, .sha256 = "36cbd247be8e689360fa37ab514d08e438c3758e30735ecfee2c7881cde7e82e"},
    {.id = "N32", .size = 681, .sha256 = "d131ce7b4c6863139aef328f8d5d87e9580257bb8318659a10da95892cd6180c"},
    {.id = "I00", .size = 607, .sha256 = "04ef100a472616f94763723fba3fb4cf108f39948c2e72b0457e5ffb5ed63166"},
    {.id = "I01", .size = 609, .sha256 = "93477e77e8173ad216aba0934c84039a842bb83fc398bac8c4ca28066e4447aa"},
    {.id = "I02", .size = 608, .sha256 = "2005ad4719f4b82d0cbad2bf3e13a9cb035a1231f429588d71bb40c4c1663825"},
    {.id = "I03", .size = 608, .sha256 = "c4baaad0f2eb6bb5b55750b5f03c303aa1a79cf4e6c07a89c5de1953c2ed7b51"},
    {.id = "I04", .size = 609, .sha256 = "58ca6b3763457458e0805e77249693fa1d585ea5bb28d8ad0ea1f372ccc36cb9"},
    {.id = "I05", .size = 609, .sha256 = "c02395be646e2678feee9a5dfad9ad41774cd755a005c9aec66fa59299a30a1e"},
    {.id = "I06", .size = 608, .sha256 = "cddf86bc8b487c21504121b8471c3d18e72dd505c3f20ca9738655bbd3451f7f"},
    {.id = "I07", .size = 609, .sha256 = "e757bb61be9a4a681dfd8d618eb18ceac353c71b72507aae5e0789f391937db4"},
    {.id = "I08", .size = 616, .sha256 = "5198d8d5e2b46310410d05152f9e1173eac704892a84250624a1ef973ac776aa"},
    {.id = "I09", .size = 617, .sha256 = "456803883dfb8ea2a5f11c2a7b231672b819d8044877581dd7446f91156f1242"},
    {.id = "I10", .size = 606, .sha256 = "55f4a27de04a2adb56c2baa778a55bd906a9b73a37d1b53236ede4cdf8eb5d55"},
    {.id = "I11", .size = 609, .sha256 = "c89696d957e5f33cff34201c4b0b5427594362aed67022d348c213338ff2ac91"},
    {.id = "I12", .size = 608, .sha256 = "b41a06ea574504ad6a1799c4d9c229ea121655fd8d1a6391e4041b97ac86631e"},
    {.id = "I13", .size = 609, .sha256 = "88d8fa27c9324d2170cd7aee1365a5b6c98a819aad5996f173cf1d14b4cb73a0"},
    {.id = "I14", .size = 607, .sha256 = "fc9ebf12e26842c1450f267d228bda452f23dbdbba0937328260f689d2c7c91f"},
    {.id = "I15", .size = 607, .sha256 = "2fa0b997b7085fe1bc7107e842e100ba66f8af77c27a64a44951223c4e61ba0d"},
    {.id = "B00", .size = 545, .sha256 = "8c3e2f5024e12f2b2db270ab86dda3b975013300bea2f0516e8ac302cc5b8875"},
    {.id = "B01", .size = 546, .sha256 = "74d753773afe4e9178ecada0fb0c711c5728a1d2429f206a7883a745b0a9dc75"},
    {.id = "B02", .size = 542, .sha256 = "2ffcc1c9e0c13b64743805e4e7ae41e7f07b6ec1833b34eb32d0d1982042d843"},
    {.id = "B03", .size = 542, .sha256 = "3219c939d4129562701ccf90e8d9e233577246630e1fd616ad02845eb4a1ddfa"},
    {.id = "B04", .size = 547, .sha256 = "a0a751112bb5dd6a66bda4a45ef1c7fd4f98bcc0cf8a7310428fe02cd3002175"},
    {.id = "B05", .size = 547, .sha256 = "c0081f8de3c4b75306df950a92c5e659f8ec7432506b35624180818556081347"},
    {.id = "B06", .size = 545, .sha256 = "ee389bf2a6dbe3387b460b42560516f04e9bd181972d04fea43b3af19acd6104"},
    {.id = "B07", .size = 542, .sha256 = "267ad58e723c322964a6f5e261a1b6a11f449ec63dba86a42c983decb3f8887f"},
    {.id = "B08", .size = 543, .sha256 = "6105a824f2004e3b34d88254f75e348d0f9caa3943e3af7cc0b43e23fa8f0849"},
    {.id = "B09", .size = 546, .sha256 = "894bf46952eb7a2e4893061e56161f7bf28e4b4cd05b1a8fd95ef1a4802e42c6"},
    {.id = "B10", .size = 546, .sha256 = "35ce3c85f3caa35add9cc54738bb87b490932898f8360f7a2d5c6e93e5ca5b8b"},
    {.id = "B11", .size = 542, .sha256 = "aa61edeb4cf16d257be17c80cac5b25849c549b3f677d2810b0dcb29c9f31084"},
    {.id = "B12", .size = 545, .sha256 = "afe0ebca95d017146c9c1ce9005ec5a628069fddfd911ecc5045f12fc5246574"},
    {.id = "B13", .size = 547, .sha256 = "96abfa512bc6181bd68a9795a2b42c74cbc9e529f28530dc240c449f0a1ac580"},
    {.id = "B14", .size = 542, .sha256 = "3ddf792d5a7de89e476d1c2adadb353cb080262850f6bab97f97b85360d85595"},
    {.id = "B15", .size = 541, .sha256 = "1f5fdeedf718faa545b45f7c0e7a478c117a0e996bb327c093dc1f0c968a7639"},
    {.id = "B16", .size = 544, .sha256 = "41fd7b75571bb253edb754f3f0211a0436e40ab5519c0eb65340f6c247ed1709"},
    {.id = "B17", .size = 543, .sha256 = "7bda236968074a3bf152f38772fc45b6c85d539351efd5e0af06d9e757e81188"},
    {.id = "B18", .size = 543, .sha256 = "1f9df6837642830357322a1dd21171c361a8745b9dcb8ce6a249b23bbbcaee9f"},
    {.id = "B19", .size = 544, .sha256 = "b468e19d176fd228d2f1557ffae23cd0ebe4e47f3f61d8a9158e0ec1158d9be1"},
    {.id = "S00", .size = 665, .sha256 = "20f80884c150bd2d6dd1938abb312fd981d2a9c2351e482c31dc79594f5511a2"},
    {.id = "S01", .size = 667, .sha256 = "5cce84088a5744c7747bd7c1105c9d16ec4bad1c9a0f2f913e1cd9a86606e362"},
    {.id = "S02", .size = 667, .sha256 = "d0484e6e9e83e60368fbfb2d891c02e80769bfa70f757e1aaead777203f6fac0"},
    {.id = "S03", .size = 667, .sha256 = "e337171b39c9504dab9de98890c7012216bde8d8a9ca40bfdce59852368c608e"},
    {.id = "S04", .size = 665, .sha256 = "07fc803dc40334fad34aac68c13a6fa3ecc4bc0224ac75c72f180a3d92ddeb5c"},
    {.id = "S05", .size = 665, .sha256 = "4bd965c36e6def199f4289d07cb44e923ad6ce9244986f1d797cbb3bcb119538"},
    {.id = "S06", .size = 665, .sha256 = "2dceca98dbca30fc5feb4ced11d04f4e3668a7f4b5a263a9679f476392a41fee"},
    {.id = "T01", .size = 666, .sha256 = "afacafdb62ab6110a29521dc733251f9f41f65b010044b21678bf8a044907460"},
    {.id = "T02", .size = 650, .sha256 = "1190006624016df64878e5981345085f85eb2fcb46d68b6a7e631955c9e86b11"},
    {.id = "T03", .size = 679, .sha256 = "bde296c79e1cc13c519ca4fe3cf34762196590d477aba40af92f571da9ffc43a"},
    {.id = "T04", .size = 665, .sha256 = "77619d5ec0566b143ced1d77199323ae97b05578766cfe6e786c23b235b04c03"},
    {.id = "T05", .size = 690, .sha256 = "60fb29633f4363986b684ffa9d90459e422ff190e73418d272af40ee660db2f6"},
    {.id = "T06", .size = 664, .sha256 = "8734434bc6109f18c45f0f329874e1035c7db0989275142ce5ed369f3214bd99"},
    {.id = "T07", .size = 789, .sha256 = "0a30d618f0c46da5f1febae64130e235e5d594fceae9bf6f1202325c3155a822"},
    {.id = "T08", .size = 651, .sha256 = "cbe6506f8506b445fb6935af05321e2a948b72a24970e7f16b10a719709e387d"},
    {.id = "T09", .size = 682, .sha256 = "23274c4249ea0a04f2b2c9f3b26d903b31e1b50c834bf37d5c157aa4260acd28"},
    {.id = "T10", .size = 668, .sha256 = "0d47a8c70d04fa4440101aeae968a402ad747da37f703e6845e459b3fea7bb55"},
    {.id = "T11", .size = 659, .sha256 = "216285bc0752c6a52f141f0cbb3800da16a0db1d8e5e7ae85964ed32320e4464"},
    {.id = "T12", .size = 660, .sha256 = "c2a1d4b5840da4b7a32783bfe4861da41a4036aeae47b3e978da568e09a0bbcd"},
    {.id = "T13", .size = 663, .sha256 = "43b3ab777bbcb9bf8806d394b063909700724d13e85d12f14bb3b66fb77e3f7d"},
    {.id = "T14", .size = 666, .sha256 = "cdfa47e9eb69b20742473e30af64c287b0e19c54faa11d788e8b2439fb95c623"},
    {.id = "T15", .size = 696, .sha256 = "9208d627abe7b3d1bc8837346053292a0eeadc65feace515d213047d855c0d1c"},
    {.id = "T16", .size = 674, .sha256 = "c36fc04cd0255ac3ce26c6e97369fa11f09eaa8d88fec7972738f45736baf419"},
    {.id = "T17", .size = 678, .sha256 = "04feb09cebfa3f6209e12941e60751ab2cd811f9eee302193c6657824be0fb66"},
    {.id = "T18", .size = 673, .sha256 = "a510cf309eebb21a26eb929b494eb39a7def76985472503b815462ad4f9e39fd"},
    {.id = "T19", .size = 678, .sha256 = "4ce020a1e70c85635282dbd90a919317215bcd2dd829f0424e0420c88dfdf4dc"},
    {.id = "T20", .size = 665, .sha256 = "57df8101cb85f2e2b5ab250e208fde45d1343e7e3fc93943c06b4dbf6c8d186f"},
    {.id = "XN00", .size = 663, .sha256 = "f6a7c5cbfaf5b42bf0a2b2f686641ade58e308a28ac72cafa76f73623848acf4"},
    {.id = "XN01", .size = 662, .sha256 = "528010377871f075d4878518374843fac94130c48ae22c70160c855642eb4f51"},
    {.id = "XN02", .size = 663, .sha256 = "07e864a0ac0ecd47e04037a330c3b328b10ab77108b05e090621b9ff4bc67d53"},
    {.id = "XN03", .size = 663, .sha256 = "85cecbdc5f663b201224a82d491788e6b0fc6a5ce8fd9dfa3583cc04f61a6c27"},
    {.id = "XN04", .size = 671, .sha256 = "e627dd26dd680d7f433fb0ee7d134f55150510dec2fe95d2b8816b37df30e678"},
    {.id = "XN05", .size = 660, .sha256 = "11ace31e103135f0c81cf12fddbcd2a5b8e8349cac0772a3530cbc6125eabd63"},
    {.id = "XN06", .size = 662, .sha256 = "2c7fd325c9caf6b61ded74ac68746b13a598066cd42cb7d15c7916e72813a937"},
    {.id = "XN07", .size = 662, .sha256 = "4a7d003d02e64494d1e8f3fc9b4d9ed6a5f3e4f9ebda2d327065f8e24f610c24"},
    {.id = "XN08", .size = 667, .sha256 = "61c5fc1f084e46ca65e079ce18f116341735730ad14f7ece5361c53ccc225d07"},
    {.id = "XN09", .size = 666, .sha256 = "7e796bf14f137a71639ef8061e2dadbe03976a762ce6c5fd2a00e89213654696"},
    {.id = "XN10", .size = 660, .sha256 = "2f1bc1a5c5cddda60a0883880747a4a321e0587badeef16bca02606e0a506958"},
    {.id = "XN11", .size = 666, .sha256 = "5f6ad49964f04a0d18044e4604772c424e78b9b50b0c0fa8ac7b9929ff9170bb"},
    {.id = "XN12", .size = 665, .sha256 = "9959ca92df1353e5c762391984f9f61ed2d03e7cdf75498a2f771182957cd9d7"},
    {.id = "XI00", .size = 609, .sha256 = "322902b10a873ba8f57159f538a8eb7aed2950c793c699b345c6bb9c4684954f"},
    {.id = "XI01", .size = 609, .sha256 = "e5cc0909ee5e81745ec767e15fae28daf5575a3fc079c0626794aae8bd1e5688"},
    {.id = "XI02", .size = 608, .sha256 = "27a97a5665d5e3feec576b50caf91ab146366b115ef497f8ac877b7a49e584af"},
    {.id = "XI03", .size = 609, .sha256 = "8b0a44f73ebab3eaf85eeed4fc7722da713179d15ef733152e7ee2b2d6b8848b"},
    {.id = "XI04", .size = 612, .sha256 = "aa8f93accbb6ec48b3286d62aa9e5274410773be64472e6651ecb57db14bc73c"},
    {.id = "XI05", .size = 608, .sha256 = "eae225224fc6f07233b681145833a5b222886ab3db030b5f0c0c21fe3a42b770"},
    {.id = "XI06", .size = 616, .sha256 = "c9c8c0df63c89788b956561e35203160677fe426d1546e9cbb3955b7fb1f6857"},
    {.id = "XI07", .size = 617, .sha256 = "4e82e5d3f1ac74bed6a4e42e6e7fc9519017de9d1b11099899a52c9e57cbc670"},
    {.id = "XI08", .size = 607, .sha256 = "106ce95853f357613621b3889707ef8cea1b6fd0fd2e2225414e0729f310d927"},
    {.id = "XI09", .size = 609, .sha256 = "631750464fdbfd24ba689bfc78e2b7c3a4a452afb6d8299357cad7f3a21ecaa3"},
    {.id = "XI10", .size = 610, .sha256 = "6d18a793da4e5bac9bd5883b9ff72cca189e03bd3e57269e13dda501fa171948"},
    {.id = "XI11", .size = 613, .sha256 = "16b293b82891d905a96784cd8201ebdad6c7e42785e92175c3977d199c3d2e5b"},
    {.id = "XI12", .size = 612, .sha256 = "ce511d0705fe253d970bf1306854b2214aa3efa4c787c8624ab7e9c055c0fd40"},
    {.id = "XI13", .size = 611, .sha256 = "fc4317136e3eccebd1af1d78c476ddf5d008a83bb91d623d8e87a66c416b2010"},
    {.id = "XI14", .size = 611, .sha256 = "aa2bc13e56cc1ed8991ca3dce9622973de3add8162f5294d6d58b626aeb67e40"},
    {.id = "XI15", .size = 612, .sha256 = "cb982e510f91b076e4e45235cfa7f710bac8ad1169ff8b2fc720a1995a13616e"},
    {.id = "XI16", .size = 616, .sha256 = "36025a56b789d7c6aa222ed2b3bdf9b6b864bee1a6845bde596861e314a575a8"},
    {.id = "XI17", .size = 612, .sha256 = "fccffb7688462e12200119f4e70d82dda39bab177df2ac5d80dd2c8b8d6f3184"},
    {.id = "XB00", .size = 542, .sha256 = "f96c6bd8df2f387f5b8c38b6175bf5cc6b6d40764ee970af30115757a3ffd59a"},
    {.id = "XB01", .size = 543, .sha256 = "35e1d9c8d3a266f6592e17bcc45741d8c765fee880ba456ecb148990a66cd252"},
    {.id = "XB02", .size = 542, .sha256 = "bf83b519c556e80d0dfcfd4dd264deb467acdcb9412e2037f05b187b2cdfb5a9"},
    {.id = "XB03", .size = 542, .sha256 = "6516397bf51e8155c48e632624bde75da6571be7be94c9d882ce8907c3e17732"},
    {.id = "XB04", .size = 543, .sha256 = "bd9a7eab24462e1bab8f64f0744648afc36a679254f1b42ea137befffaa8ca8f"},
    {.id = "XB05", .size = 544, .sha256 = "9f2b1555c0e00893d0007f0046b5f0bcf97f37838fb4ee530bb2e7261478fe79"},
    {.id = "XB06", .size = 547, .sha256 = "54dd22773d6d112592b983c35f9e6c5d67637867bc6a17a21dfb53dc22e7d86a"},
    {.id = "XB07", .size = 546, .sha256 = "c9008cdb08905f048ff855f20d31c2f1e1e6c1f63a896b13af7589e91580522e"},
    {.id = "XB08", .size = 547, .sha256 = "1fc79290a0335960f6cbd230d7f5283857c921db98dab7a1a332097335476ab7"},
    {.id = "XB09", .size = 544, .sha256 = "4fd7495aca67ade4de55c1c8c8c82f4817a8e154f6b224967f96212019f026d7"},
    {.id = "XB10", .size = 543, .sha256 = "270daf1330348cd571b7ba0fa881f096bd72d67866d2880d8b9f9000cf026c50"},
    {.id = "XB11", .size = 542, .sha256 = "11e43c3a500d1cb1c38e854d77fb222a98df2294fb7b17c858452d911c167695"},
    {.id = "XB12", .size = 546, .sha256 = "574b034eba35ac2136210cd5bb08109ea8dee274601cc09f0563f0c6b64c4681"},
    {.id = "XB13", .size = 548, .sha256 = "b5c5547606a1f7586832b18f2946353544db5b32534f9d3ddbbdfe176a25775e"},
    {.id = "XB14", .size = 543, .sha256 = "6d136ce24c3d439aec9a527d4619fcf5acdd4307ef5b49521b560b2780634cb2"},
    {.id = "XB15", .size = 542, .sha256 = "67cc9cc2b78866dbf8ed8968eca55004e17d93caa3d881edb32cc8a1627efb2d"},
    {.id = "XB16", .size = 550, .sha256 = "1a811f345a548bc515ff872708a0b9bfa12845834ad2827652be113d268d3c29"},
    {.id = "XB17", .size = 550, .sha256 = "f283c78800efbe9babfeb04d7532da3a2d6a1c4c2713a87fcbb315a7b7d4ff49"},
    {.id = "XB18", .size = 549, .sha256 = "c5c9e98044478e7826faa6ac113732c2bf7b6da240c131efac968d4813224abc"},
    {.id = "XB19", .size = 550, .sha256 = "02f600c65992d6b545803e08e65fbf02ca92cfbbbb6e9b7a404b2ce55a5e5505"},
    {.id = "XB20", .size = 550, .sha256 = "1e25d39d6a920064359d46d418c439010eb0a1df183bfee746b12d858e0da85a"},
    {.id = "XB21", .size = 551, .sha256 = "9076af23df1bf0a3a96545af78a6019ea57ec0058c4cac028250fbedce4d470b"},
    {.id = "XB22", .size = 553, .sha256 = "2ddfec1cfd4e331ffb3ede662c791962fa2dd160513cab879988d29f7e553b96"},
    {.id = "XB23", .size = 553, .sha256 = "b7f7117134055a7f7ff54d783a3608e4f8e4dedd678b6f612d3b45a47c9a8237"},
    {.id = "XB24", .size = 551, .sha256 = "4c505a4434f85e3b7c1bae4e434b7f062b6f0b08ea0e5d3270a6dadfec3d8c5c"},
    {.id = "XB25", .size = 552, .sha256 = "6c108b4087251cd71195a5bd30fdde1c5a3a26a4715015e14655c2dfd62724e4"},
    {.id = "XB26", .size = 550, .sha256 = "cc0be3212981318017b77ff7edfa9564f997cfae0902cac089bc8f11c6b4a425"},
    {.id = "XB27", .size = 551, .sha256 = "9b79f48e1bb75b48b67c0aeefbf15d5292b53aa6166c1faaddc08c3c6fd6fbf6"},
    {.id = "XB28", .size = 552, .sha256 = "d76900f9c753fc62e0e644851e1cce0b4216594d7b4c2c132da82668b2ed5ef0"},
    {.id = "XB29", .size = 551, .sha256 = "c1b82339e18c0b7cf55bfb043e6765859a8fef65f118fda733a8e405da610dae"},
    {.id = "XB30", .size = 551, .sha256 = "ca851c1964b309b3f61164296443b2e65b956845358e20dc23a5ab86b49198d7"},
    {.id = "XB31", .size = 551, .sha256 = "36cb797b3b38ccd6fc187ecabe887f8bf9c93bd07e75bfe18ab52f7162c6a08a"},
    {.id = "XB32", .size = 551, .sha256 = "abf4c7244809dfe444ca1f31c83cdb7078f74fa40926ab1219fdab798ea16256"},
    {.id = "XB33", .size = 543, .sha256 = "e15d5209db1c922d7ecdb697730f6fa91b2037ffd333ffabe938270a76b56d29"},
    {.id = "XB34", .size = 545, .sha256 = "ed50b96096e0088e381cf449ae1ff4a0afcd747d77fbd4db442de5256564082d"},
    {.id = "XB35", .size = 543, .sha256 = "119ec0529edad350d467fa4dd55b8e85dbe5c745b9be0662e43ae5c15e4fb864"},
    {.id = "XS00", .size = 677, .sha256 = "b30412bd6e1b68c3e478c5da7710f4c2fb4e22e6320018b6cbbfa8680bd01c30"},
    {.id = "XS01", .size = 675, .sha256 = "706b8670ce017cd556d8ffb4e84047f3baae2d57775f1d58406dc0b0064c8927"},
    {.id = "XS02", .size = 670, .sha256 = "3ddc581ac441aecaf4e161a7665636bf186b7131fc2d6b9a4253eacd596be3e5"},
    {.id = "XS03", .size = 669, .sha256 = "4529311c036d8f3171a263036653a9d43fe9d4f70c6d52a030489c788b7e5c42"},
    {.id = "XS04", .size = 660, .sha256 = "d47211118f163dc5350296739075fd8301aa6295d192c00366d92cdc4f7973d6"},
    {.id = "XS05", .size = 669, .sha256 = "29846afcf8e84a90ba155b07ec9eee3cabf82c6d59db3f61cde7b173a2c2a30a"},
    {.id = "XS06", .size = 673, .sha256 = "7d3af6f2439976a05d273f428b37f5ff003784edff25b2aeb162eaba7c8d503f"},
    {.id = "XT01", .size = 670, .sha256 = "d94c0dfd71e7d9cd1e9088cd4471f8472be0273ef54c39291b20fc626299a5fd"},
    {.id = "XT02", .size = 666, .sha256 = "88705a6a2dcb05b8e0fac5c2d071a2b63412b30d6c6115d4bcab6c5a9d40648a"},
    {.id = "XT03", .size = 670, .sha256 = "2ffd9ba72f5c3a466375301c1ab6a06387894ea99822e83fa0c8f82447f5e9d4"},
    {.id = "XT04", .size = 672, .sha256 = "69867c9f9af6da207daece181e6a25cf69b026f11b94fe794e1571bb0f6307d2"},
    {.id = "XT05", .size = 672, .sha256 = "ae9dd46dd6ae973935905959a87c137e3de27c45c2c378f4844fb94e22050ce7"},
    {.id = "XT06", .size = 678, .sha256 = "c17c14d795aa83fc327cfd495d2152f1142a79412bb6e5d72ad06670d12ab6fa"},
    {.id = "XT07", .size = 676, .sha256 = "3caa6681c5296d816cd6e0e18b8eec6e839ac52c70b11d9acd4dc8768eef1d87"},
    {.id = "XT08", .size = 676, .sha256 = "3490951bfefa87da3b4e0dde54cc5c8e35487ed70f00d02b21d7c620c9afa0dd"},
    {.id = "XT09", .size = 733, .sha256 = "606d283fb87557b8631b96e1e07cdf5fc704ec08a8e3fa7fe73de4375cbd38e6"},
    {.id = "XT10", .size = 733, .sha256 = "3b555e0c4a151683ccf6b27b4b73aa96851bda980086abcc1cb8cef54a38c3ba"},
    {.id = "XT11", .size = 669, .sha256 = "ba6275be0a1910e4fdcd1e0ccc63ced59a215ce3e040b290da5811e3ef3bfd4b"},
    {.id = "XT12", .size = 665, .sha256 = "e7dddf15e3abf8449e931b98f6baf0b8cebd21ec4e2964f3e9f6eb248410bf0f"},
    {.id = "XT13", .size = 668, .sha256 = "34f86b2ff3b1ceeb9415005fcd1f0ef4b3e70c307ace1bbec9716be6b546ac6d"},
    {.id = "XT14", .size = 692, .sha256 = "6091d4023153f2d4aca68b08647c199f4ea4b5e5048219e006fa054f7c20e0bb"},
    {.id = "XT15", .size = 668, .sha256 = "6b022b5b57c88b3dd4c7078b2ee68e2a9067efa8c5e54389acbbf51187ce4a05"},
    {.id = "XT16", .size = 666, .sha256 = "9ef08da8aaa360bdbf249611e90aeee4e1adebc51cf624d394f060a140234991"},
    {.id = "XT17", .size = 665, .sha256 = "276c693061c259612bee61e0e2fec8355a3e9027e0f74cc6742b06e7a89b15be"},
    {.id = "XT18", .size = 679, .sha256 = "e7cdcc060f83e2685a1ba2c1a5f581e7001c4ed4af78bbfedf9e6059ee87b8a2"},
    {.id = "XT19", .size = 673, .sha256 = "be223366711822f518f24a01b472e7b53ea32393e3e05ac7a3c33ecfb0d093dc"},
    {.id = "XT20", .size = 426, .sha256 = "fa28efb74b3e2abbfca52529d98d8f2232467f8deff3734b5eaa4f944e705dc7"},
    {.id = "XT21", .size = 419, .sha256 = "16ac9ead8aa33a86e3691b40999a4796e9ce8e1326de3f2be4e20b1e85a63abc"},
    {.id = "XT22", .size = 407, .sha256 = "fc0d58646b13a55a57d896f6d40d4afbf5d3540317bb87a705b41dad4f04d14a"},
    {.id = "XT23", .size = 78, .sha256 = "6a035d5ee9e09f61177e0bc4d1b61b4204febbc315d9ebef97a0234a7d85f3a3"},
    {.id = "XT24", .size = 683, .sha256 = "02fce8c14c766210fa43b736b20f14c31282fdbb03745072629eaa11dc7c1b53"},
    {.id = "XT25", .size = 667, .sha256 = "e7abd28b2facfea7c7a557e7ebbb603e5f4b0a07ae0f21a21d8e6ee65724b2a3"},
    {.id = "XT26", .size = 667, .sha256 = "42c004b1ede0233977ae545e41d3db8b87803acab547508ec0d09f4081589a6e"},
    {.id = "XT27", .size = 667, .sha256 = "ef1ded0f002d55fba5ee572ca1fb4e80b1ea2157964a313d48f62d31c6880568"},
    {.id = "XT28", .size = 612, .sha256 = "92a80b5c23588cfa5809cf4bee49bbec13b257e86d1ba0badce08b8ff7f35c3f"},
    {.id = "XT29", .size = 547, .sha256 = "4f742a6bc2f5f95fb190b85058d5691d7f66742943c73776773d8bb5bebab6db"},
    {.id = "XT30", .size = 692, .sha256 = "1e99770de32c145279f17ddae81f4e97134ce42b369a74047bf45240f1b234d5"},
    {.id = "XT31", .size = 623, .sha256 = "0c76bdbc27b0d5b99df42aa579c7baf9bf0424a02e34b69e0d45ef0c83cc9b6b"},
    {.id = "XT32", .size = 622, .sha256 = "5f566bc00eb3b1b178b74da586f45239a20b4f5eea012129eb9848568db7d141"},
    {.id = "XT33", .size = 684, .sha256 = "055a897b6d0b40ba86505c80a605d8c8a1f70c6e51230f2dd6078725e9e9b0d3"},
    {.id = "XT34", .size = 563, .sha256 = "6f9b27240fcc773f73be9d332be16a35f673cbd6525c7b2e929484a956765cfb"},
    {.id = "XT35", .size = 691, .sha256 = "1622155e8215becc6fe31ba74fa5b19b687c98f110fc3e452b4fb8dc01040c2e"},
    {.id = "XT36", .size = 685, .sha256 = "f10ce3f687627ee448a2c60d97d6bfe16663745077ba02f89f5efd3ef14d6465"},
    {.id = "XT37", .size = 726, .sha256 = "5976018fb0532e86df6cd2528592d3a5a4db94bdce8ba7fa355e1bebb3a9d5d9"},
    {.id = "XT38", .size = 669, .sha256 = "a2c71e05ada536024f8e704315824e715758d0f522601c9fdbb2f9c00ac36398"},
    {.id = "XT39", .size = 683, .sha256 = "c458063d3dc4b9d8ac47bcfd6f2df863b07f1e384e339cd5c353cdc870c8ea6b"},
    {.id = "XT40", .size = 684, .sha256 = "a2c05d4f5e3bf72a5f035acc2a732ca42a28c2db785ab6f2644813de68f6ff75"},
    {.id = "XT41", .size = 725, .sha256 = "47edda8c440099e7f84e102d1d84e90f1a2782c4739fa3d5fdd51c7dfc56b5f7"},
    {.id = "XT42", .size = 701, .sha256 = "53cde73264ae92d4b0d3e178fae00c5560bd896990566e7806dc6cae81ec08bf"},
    {.id = "XT43", .size = 694, .sha256 = "ce3082920e03b5b1bd0c66707032d340726575cb8886b7d151725171e1e2798c"},
    {.id = "XT44", .size = 692, .sha256 = "07fb7ad059d2636ba1d4ba0fe71f582c9fe490bcf49fe3a19cf947785575253d"},
    {.id = "XT45", .size = 669, .sha256 = "35844564d92788145c037b8f7c8b9f977155bfe7308e50e9387793e6cac74a80"},
    {.id = "XT46", .size = 669, .sha256 = "eebe388b459f70080a5bff3df82187ca95a1afbe175716f73c64e51f246755bb"},
    {.id = "XT47", .size = 673, .sha256 = "4b513ae83ba7343a32474157c546a42547173ab78a743013fa1a7a0782891285"},
    {.id = "XT48", .size = 683, .sha256 = "42aea76e375c09cb61582e470f8b9b20e8df48e164dd1900fa4e977408f22d90"},
    {.id = "XH00", .size = 694, .sha256 = "0974c56c6973288168b7933f151d7a97e9ffec6068ce749353138596050242a0"},
    {.id = "XH01", .size = 696, .sha256 = "65cc502ff6f3a88d06b83513d76515e3ffaf67fcaa6ceb4e87702ada984a98cd"},
    {.id = "XH02", .size = 696, .sha256 = "06e25d3ea8ecaabcd3066ca26090ab85cfc3e33b99ea2d68e7968f3f2d363b36"},
    {.id = "XH03", .size = 687, .sha256 = "9b00191de476d538c0fd5fc65c27037e0ee0d57827cc707ec11e862bbeb794e5"},
    {.id = "XH04", .size = 691, .sha256 = "859b63e5bdb00d719cab34f140a3582beb8212e09f2e751990d9374ecc2461b3"},
    {.id = "XH05", .size = 689, .sha256 = "27e9a35bb29127dd75e6a4db750cb40753fa9d8e50efbf095738da3f82690423"},
    {.id = "XH06", .size = 688, .sha256 = "049d1c22c3a9230cb19dd92bc90ef3b9b3e0a31194eabeedb08baee85826e07a"},
    {.id = "XH07", .size = 696, .sha256 = "f48352be5a4c2d5fec735a675076a76b6240091f0590a17d0a96623c10cd1dc1"},
    {.id = "XH08", .size = 691, .sha256 = "f0910b4b00176713813128d975ea768cb8ff352c9a00da3df8aa9867bb9dddd8"},
    {.id = "XH09", .size = 691, .sha256 = "04bfb5f8a7350d385d55170a68dbdaf4737affd0ca4279f374110ae26ebf1909"},
    {.id = "XH10", .size = 693, .sha256 = "718b8a1282867979edcd424c6be981a6a762154a08364f74336edbf4c933c0a7"},
    {.id = "XH11", .size = 692, .sha256 = "c187fd478eec559e0ff883bd9839a107d9cea6b38383ee1de3dc363722b8d86f"},
    {.id = "XH12", .size = 692, .sha256 = "5930122ca47ee925069bf6631614b072e3d35690236f94371f388f534a7c00a9"},
    {.id = "XH13", .size = 691, .sha256 = "dfb5f089749e4dc16cd636b8717651f0a7eda9274ce90d2d52b1a679dae35657"},
    {.id = "XH14", .size = 691, .sha256 = "31f95a447c185cf74de76fc9c317ad4a5e6552614f179f576e696dec26437399"},
    {.id = "XH15", .size = 691, .sha256 = "736924cb946701a98758235c918b62d938840ec5caf8d142e25cbefbe970683f"},
    {.id = "XH16", .size = 695, .sha256 = "c67b50b44085a0751565231b1d64cb9bd4dddf8a0dbe38d698c8edd65833af0d"},
    {.id = "XH17", .size = 691, .sha256 = "5bbd0c424ee6d50aa601138c63701ce26d7ccc140d218fb0254341a32e0a11f5"},
    {.id = "XH18", .size = 691, .sha256 = "fc78ecd0bd1cab13bde6d73276178e8e666652371f416d6cbcadbf2ce06e9ab1"},
    {.id = "XH19", .size = 691, .sha256 = "9a27ffb018d12f0fd23992856ad28dde6939c0b501e7ec7b6570e8d9d2274c6b"},
    {.id = "XH20", .size = 695, .sha256 = "3ed5c84f673c30640180c4390f80b074c58e9a598d32f79280fa3a9873072c7c"},
    {.id = "XH21", .size = 695, .sha256 = "9807555bd0dc95ab13a3366aaba3fc0ed6f312611a2757ea210bb5d2083d99b5"},
    {.id = "XH22", .size = 691, .sha256 = "a5d9cc5d39c9a125579fc17cea894e478c61923f4401154d5dafc25acd0b5d0a"},
    {.id = "XH23", .size = 691, .sha256 = "a9eff09822463255643f7629da8f39dfaebcc44dc7f08f28d5fe62396011bac3"},
    {.id = "XH24", .size = 691, .sha256 = "8e62cbd2ca5ac41148740d96d39fb8ae514b8305bf102210444f31fd0664a84c"},
    {.id = "XH25", .size = 691, .sha256 = "da29830e90991aed66cbb665b707892bffd51e0c0ea1abcfb88eed20b7394ea9"},
    {.id = "XH26", .size = 691, .sha256 = "5c8e7321561aea1c890d5130c48b05b9e6397b1552a141c000d6b9382d1106af"},
    {.id = "XH27", .size = 691, .sha256 = "d6e9dc737f10510a283fe18ce38333e4d9e71a5947f7483a3c112109a48f1fe7"},
    {.id = "XH28", .size = 690, .sha256 = "e2ef0a487dc73c7a86277515d35847de66a7e27cc31ebec6372ae6054141928a"},
    {.id = "XH29", .size = 691, .sha256 = "002c7f72e9865ac6b006759edbf344486204589f554557198a06fa697afe72fa"},
    {.id = "XH30", .size = 691, .sha256 = "a23b43dfd494179fee8f879274e8662138e4866286ae0faa8c9e7306d86ac57b"},
    {.id = "XH31", .size = 691, .sha256 = "dccba52df1435d4f3397c0a2b91f8f165db556a8ae9674ecf4a82844dab59e48"},
    {.id = "XH32", .size = 692, .sha256 = "83e7f2f5fd41f624fe93069d231f568301d9b12251c62b941b209f744a828f97"},
    {.id = "XH33", .size = 695, .sha256 = "b28038fca991785cb10d631cfd65ac4f588a31642eb3f2a583ca57eff7c90f79"},
    {.id = "XH34", .size = 696, .sha256 = "69a136e890d1d429e0407e75d056ae915b5a06532137658631ef00828b99f8fb"},
    {.id = "XH35", .size = 691, .sha256 = "234f9ef2179ef360f43fd15aeaf1d6d2c2ccf8ae3bc7bfa0eeb62716576cca47"},
    {.id = "XH36", .size = 691, .sha256 = "6706b7c4bce3aea048a156061ad4464dbcefc6bf7894d66443765a230be44550"},
});
// clang-format on

/// The parsed document of the case @p id, kept for its elements.
class ParsedCase
{
public:
    explicit ParsedCase(std::string_view id) : m_document(OrcDocument::parse(orcEdgeCaseBytes(id)))
    {
        EXPECT_TRUE(m_document.has_value())
            << id << ": " << (m_document ? std::string() : m_document.error().toString());
    }

    [[nodiscard]] std::optional<OrcElement> root() const
    {
        return m_document ? std::optional<OrcElement>(m_document->root()) : std::nullopt;
    }

    /// The last element @p kind of the last <Components>.
    [[nodiscard]] std::optional<OrcElement> component(std::string_view kind) const
    {
        const std::optional<OrcElement> components = wrapper("Components");
        return components ? components->lastChild(kind) : std::nullopt;
    }

    /// The last <Material> of the last <Materials>, the first of the two the probe declares
    /// (Cardboard) when the case changed nothing.
    [[nodiscard]] std::optional<OrcElement> firstSectionMaterial() const
    {
        const std::optional<OrcElement> materials = wrapper("Materials");
        if (!materials)
        {
            return std::nullopt;
        }
        const std::vector<OrcElement> all = materials->children("Material");
        return all.empty() ? std::nullopt : std::optional<OrcElement>(all.front());
    }

    /// The last @p name under the root.
    [[nodiscard]] std::optional<OrcElement> wrapper(std::string_view name) const
    {
        const std::optional<OrcElement> top = root();
        return top ? top->lastChild(name) : std::nullopt;
    }

private:
    Result<OrcDocument> m_document;
};

/// The element @p name of the component @p kind of the case @p id.
[[nodiscard]] std::optional<OrcElement> leafOf(const ParsedCase& parsed, std::string_view kind,
                                               std::string_view name)
{
    const std::optional<OrcElement> component = parsed.component(kind);
    return component ? component->lastChild(name) : std::nullopt;
}

/// The double the leaf @p name of the component @p kind of the case @p id holds (its XmlValue),
/// nullopt when the text gives none (or there is no such leaf: see hasLeaf()).
[[nodiscard]] std::optional<double> numberOf(std::string_view id, std::string_view kind,
                                             std::string_view name)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> leaf = leafOf(parsed, kind, name);
    return leaf ? leaf->doubleValue() : std::nullopt;
}

/// The body tube's Length of the case @p id.
[[nodiscard]] std::optional<double> lengthOf(std::string_view id)
{
    return numberOf(id, "BodyTube", "Length");
}

[[nodiscard]] bool hasLeaf(std::string_view id, std::string_view kind, std::string_view name)
{
    const ParsedCase parsed(id);
    return leafOf(parsed, kind, name).has_value();
}

/// The attribute @p attribute of the leaf @p name of the component @p kind.
[[nodiscard]] std::optional<std::string> attributeOf(std::string_view id, std::string_view kind,
                                                     std::string_view name,
                                                     std::string_view attribute)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> leaf = leafOf(parsed, kind, name);
    if (!leaf)
    {
        return std::nullopt;
    }
    const std::optional<std::string_view> value = leaf->attribute(attribute);
    return value ? std::optional<std::string>(*value) : std::nullopt;
}

[[nodiscard]] std::optional<std::string> lengthUnitOf(std::string_view id)
{
    return attributeOf(id, "BodyTube", "Length", "Unit");
}

/// The String field @p name of the component @p kind.
[[nodiscard]] std::optional<std::string> stringOf(std::string_view id, std::string_view kind,
                                                  std::string_view name)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> component = parsed.component(kind);
    if (!component)
    {
        return std::nullopt;
    }
    const std::optional<std::string_view> value = component->stringProperty(name);
    return value ? std::optional<std::string>(*value) : std::nullopt;
}

[[nodiscard]] std::optional<std::string> tubeStringOf(std::string_view id, std::string_view name)
{
    return stringOf(id, "BodyTube", name);
}

/// The parachute's Integer field @p name (Sides, LineCount).
[[nodiscard]] std::optional<int> parachuteIntOf(std::string_view id, std::string_view name)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> parachute = parsed.component("Parachute");
    return parachute ? parachute->integerProperty(name) : std::nullopt;
}

[[nodiscard]] std::optional<int> sidesOf(std::string_view id)
{
    return parachuteIntOf(id, "Sides");
}

/// The bulkhead's Boolean field Filled.
[[nodiscard]] std::optional<bool> filledOf(std::string_view id)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> bulkhead = parsed.component("BulkHead");
    return bulkhead ? bulkhead->booleanProperty("Filled") : std::nullopt;
}

/// The nose cone's enum field Shape: the index among kShapes.
[[nodiscard]] std::optional<std::size_t> shapeOf(std::string_view id)
{
    const std::optional<std::string> text = stringOf(id, "NoseCone", "Shape");
    return text ? QtRocket::JaxbLexical::parseEnum(*text, kShapes) : std::nullopt;
}

/// The primitive double Density of the first section material.
[[nodiscard]] std::optional<double> densityOf(std::string_view id)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> material = parsed.firstSectionMaterial();
    return material ? material->doubleProperty("Density") : std::nullopt;
}

/// The Double ShearModulus of the first section material.
[[nodiscard]] std::optional<double> shearModulusOf(std::string_view id)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> material = parsed.firstSectionMaterial();
    return material ? material->boxedDoubleProperty("ShearModulus") : std::nullopt;
}

/// A String field of the first section material.
[[nodiscard]] std::optional<std::string> sectionStringOf(std::string_view id, std::string_view name)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> material = parsed.firstSectionMaterial();
    if (!material)
    {
        return std::nullopt;
    }
    const std::optional<std::string_view> value = material->stringProperty(name);
    return value ? std::optional<std::string>(*value) : std::nullopt;
}

[[nodiscard]] std::optional<std::string> sectionUomOf(std::string_view id)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> material = parsed.firstSectionMaterial();
    if (!material)
    {
        return std::nullopt;
    }
    const std::optional<std::string_view> value = material->attribute("UnitsOfMeasure");
    return value ? std::optional<std::string>(*value) : std::nullopt;
}

/// The number of components @p kind (in no namespace) in the last <Components>.
[[nodiscard]] std::size_t componentCount(std::string_view id, std::string_view kind)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> components = parsed.wrapper("Components");
    return components ? components->children(kind).size() : 0;
}

/// Whether the root of the case @p id is an .orc file's: OpenRocketComponent in no namespace.
[[nodiscard]] bool hasOrcRoot(std::string_view id)
{
    const ParsedCase                parsed(id);
    const std::optional<OrcElement> root = parsed.root();
    return root && root->isNamed("OpenRocketComponent");
}

[[nodiscard]] bool hasWrapper(std::string_view id, std::string_view name)
{
    const ParsedCase parsed(id);
    return parsed.wrapper(name).has_value();
}

/// The failure of reading the case @p id: its code and message (nullopt when it parses).
[[nodiscard]] std::optional<std::string> failureOf(std::string_view id)
{
    const Result<OrcDocument> document = OrcDocument::parse(orcEdgeCaseBytes(id));
    if (document)
    {
        return std::nullopt;
    }
    return std::string(QtRocket::toString(document.error().code)) + ": " + document.error().message;
}

[[nodiscard]] bool isNaN(std::optional<double> value)
{
    return value && std::isnan(*value);
}

[[nodiscard]] bool isNegativeZero(std::optional<double> value)
{
    return value && *value == 0.0 && std::signbit(*value);
}

TEST(OrcEdgeCases, AreTheProbesDocuments)
{
    ASSERT_EQ(orcEdgeCases().size(), kPinnedDocuments.size());
    std::size_t index = 0;
    for (const PinnedDocument& pinned : kPinnedDocuments)
    {
        const QtRocket::Test::OrcEdgeCase& edgeCase = orcEdgeCases().at(index);
        EXPECT_EQ(edgeCase.id, pinned.id) << "case " << index;
        EXPECT_EQ(edgeCase.bytes.size(), pinned.size) << edgeCase.name;
        EXPECT_EQ(QtRocket::sha256Hex(orcEdgeCaseBytes(edgeCase.id)), pinned.sha256)
            << edgeCase.name;
        index++;
    }
}

TEST(OrcEdgeCases, ReadTheBaseline)
{
    // 01: Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' ...];
    // InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572 (in inches).
    EXPECT_TRUE(hasOrcRoot("01"));
    EXPECT_EQ(componentCount("01", "BodyTube"), 1U);
    EXPECT_EQ(tubeStringOf("01", "Manufacturer"), "Estes");
    EXPECT_EQ(tubeStringOf("01", "PartNumber"), "BT-X");
    EXPECT_EQ(tubeStringOf("01", "Description"), "d");
    EXPECT_EQ(tubeStringOf("01", "Material"), "Cardboard");
    EXPECT_EQ(attributeOf("01", "BodyTube", "Material", "Type"), "BULK");
    EXPECT_EQ(numberOf("01", "BodyTube", "InsideDiameter"), 0.95);
    EXPECT_EQ(attributeOf("01", "BodyTube", "InsideDiameter", "Unit"), "in");
    EXPECT_EQ(numberOf("01", "BodyTube", "OutsideDiameter"), 0.976);
    EXPECT_EQ(lengthOf("01"), 18.0);
    EXPECT_EQ(lengthUnitOf("01"), "in");
    EXPECT_EQ(sectionStringOf("01", "Name"), "Cardboard");
    EXPECT_EQ(densityOf("01"), 680.0);
    EXPECT_EQ(sectionUomOf("01"), "kg/m3");
    EXPECT_EQ(sectionStringOf("01", "Type"), "BULK");
    EXPECT_EQ(shearModulusOf("01"), std::nullopt);
    EXPECT_EQ(stringOf("01", "BodyTube", "Thumbnail"), std::nullopt);
}

TEST(OrcEdgeCases, Batch1Structure)
{
    // 02: the unknown <Color> is a child, which no DTO field names: the same preset as 01.
    EXPECT_EQ(tubeStringOf("02", "Color"), "red");
    EXPECT_EQ(lengthOf("02"), 18.0);
    // 03: the unknown <FinSet> is skipped, the body tube read.
    EXPECT_EQ(componentCount("03", "FinSet"), 1U);
    EXPECT_EQ(componentCount("03", "BodyTube"), 1U);
    // 04, 05, 06: THROWS NullPointerException (this.length is null; str is null), and a preset
    // with PartNo=<null>.
    EXPECT_FALSE(hasLeaf("04", "BodyTube", "Length"));
    EXPECT_EQ(tubeStringOf("05", "Manufacturer"), std::nullopt);
    EXPECT_EQ(tubeStringOf("06", "PartNumber"), std::nullopt);
    EXPECT_EQ(tubeStringOf("06", "Description"), std::nullopt);
    // 20 to 24: the Unit attribute as written (20, 24: THROWS IllegalArgumentException:
    // name=furlong, name= in; 21: Length=18.0, the default unit m; 22: 0.4572 in 64ths;
    // 23: 5.486400000000001).
    EXPECT_EQ(lengthUnitOf("20"), "furlong");
    EXPECT_EQ(lengthUnitOf("21"), std::nullopt);
    EXPECT_EQ(lengthOf("21"), 18.0);
    EXPECT_EQ(lengthUnitOf("22"), "in/64");
    EXPECT_EQ(lengthUnitOf("23"), "ft");
    EXPECT_EQ(lengthUnitOf("24"), " in");
    // 25: Length=0.508, the second of two (20 in).
    EXPECT_EQ(lengthOf("25"), 20.0);
    // 26 to 31: the Material element (26: 'Unobtainium' 0.0 Other user; 27, 28: THROWS
    // IllegalArgumentException: Invalid material type FOO / null specified for Component; 29:
    // 'Paper test' 800.0; 30: 'Cardboard' builtin; 31: no Material).
    EXPECT_EQ(tubeStringOf("26", "Material"), "Unobtainium");
    EXPECT_EQ(attributeOf("27", "BodyTube", "Material", "Type"), "FOO");
    EXPECT_EQ(attributeOf("28", "BodyTube", "Material", "Type"), std::nullopt);
    EXPECT_EQ(tubeStringOf("28", "Material"), "Cardboard");
    EXPECT_EQ(tubeStringOf("29", "Material"), "Paper test");
    EXPECT_EQ(tubeStringOf("30", "Material"), " Cardboard ");
    EXPECT_FALSE(hasLeaf("31", "BodyTube", "Material"));
    // 32: Mass=0.01 (10 g); 33: THROWS IllegalArgumentException: name=stone.
    EXPECT_EQ(numberOf("32", "BodyTube", "Mass"), 10.0);
    EXPECT_EQ(attributeOf("32", "BodyTube", "Mass", "Unit"), "g");
    EXPECT_EQ(attributeOf("33", "BodyTube", "Mass", "Unit"), "stone");
}

TEST(OrcEdgeCases, Batch1Documents)
{
    // 34, 37: THROWS IOException: Unable to parse component preset file: case.orc (invalid format
    // or corrupted file): well-formed, but the root is no OpenRocketComponent of no namespace.
    EXPECT_EQ(failureOf("34"), std::nullopt);
    EXPECT_FALSE(hasOrcRoot("34"));
    EXPECT_EQ(failureOf("37"), std::nullopt);
    EXPECT_FALSE(hasOrcRoot("37"));
    // 35, 36: the same IOException, for what the parser refuses (OpenRocket logs its message).
    EXPECT_EQ(failureOf("35"),
              "PARSE: XML document structures must start and end within the same entity.");
    EXPECT_EQ(failureOf("36"), "PARSE: Premature end of file.");
    // 38 to 40: Version is never read (any value or none); Legacy=true for <Legacy>no</Legacy>.
    EXPECT_EQ(stringOf("38", "BodyTube", "PartNumber"), "BT-X");
    EXPECT_FALSE(hasWrapper("38", "Version"));
    EXPECT_TRUE(hasWrapper("39", "Version"));
    EXPECT_TRUE(hasWrapper("40", "Legacy"));
    EXPECT_FALSE(hasWrapper("01", "Legacy"));
    // 41, 42: OK 0 presets (no <Components>, a body tube outside it).
    EXPECT_FALSE(hasWrapper("41", "Components"));
    EXPECT_FALSE(hasWrapper("42", "Components"));
    EXPECT_TRUE(hasWrapper("42", "BodyTube"));
    // 43: PartNo=BT-Y, the second wrapper's list.
    EXPECT_EQ(tubeStringOf("43", "PartNumber"), "BT-Y");
    EXPECT_EQ(componentCount("43", "BodyTube"), 1U);
    // 44: the IOException of an unknown unit of density (lb/in3, whose 3 OpenRocket makes a
    // superscript three); 45: 'Cardboard' 0.0 Other user; 46: 680.0, no unit; 47, 48: the Group
    // (Rocks is null); 49: THROWS NullPointerException, MaterialDTO.getType() is null.
    EXPECT_EQ(sectionUomOf("44"), "lb/in3");
    EXPECT_EQ(densityOf("45"), std::nullopt);
    EXPECT_EQ(sectionUomOf("46"), std::nullopt);
    EXPECT_EQ(densityOf("46"), 680.0);
    EXPECT_EQ(sectionStringOf("47", "Group"), "Woods");
    EXPECT_EQ(sectionStringOf("48", "Group"), "Rocks");
    EXPECT_EQ(sectionStringOf("49", "Type"), "ROCK");
    EXPECT_EQ(QtRocket::JaxbLexical::parseEnum("ROCK", kMaterialTypes), std::nullopt);
    // 50 to 52: no Filled, Filled=true, no Filled.
    EXPECT_EQ(filledOf("50"), std::nullopt);
    EXPECT_EQ(filledOf("51"), true);
    EXPECT_EQ(filledOf("52"), std::nullopt);
}

TEST(OrcEdgeCases, Batch1Components)
{
    // 53: Diameter=0.30479999999999996 (12 in); LineCount=6; LineLength 12 in; Sides=6;
    // Material=[Surface 'Ripstop nylon' ...]; LineMaterial=[Line 'Carpet Thread' ...].
    EXPECT_EQ(numberOf("53", "Parachute", "Diameter"), 12.0);
    EXPECT_EQ(sidesOf("53"), 6);
    EXPECT_EQ(parachuteIntOf("53", "LineCount"), 6);
    EXPECT_EQ(numberOf("53", "Parachute", "LineLength"), 12.0);
    EXPECT_EQ(stringOf("53", "Parachute", "Material"), "Ripstop nylon");
    EXPECT_EQ(attributeOf("53", "Parachute", "Material", "Type"), "SURFACE");
    EXPECT_EQ(stringOf("53", "Parachute", "LineMaterial"), "Carpet Thread");
    EXPECT_EQ(attributeOf("53", "Parachute", "LineMaterial", "Type"), "LINE");
    // 54 to 56: LineCount=<null>.
    EXPECT_EQ(parachuteIntOf("54", "LineCount"), std::nullopt);
    EXPECT_EQ(parachuteIntOf("55", "LineCount"), std::nullopt);
    EXPECT_EQ(parachuteIntOf("56", "LineCount"), std::nullopt);
    // 57: THROWS IOException ... InvalidComponentPresetException (no LineLength).
    EXPECT_FALSE(hasLeaf("57", "Parachute", "LineLength"));
    // 58: DragCoefficient=0.02032, read as a length of 0.8 in.
    EXPECT_EQ(numberOf("58", "Parachute", "DragCoefficient"), 0.8);
    EXPECT_EQ(attributeOf("58", "Parachute", "DragCoefficient", "Unit"), "in");
    // 59, 60: the materials of other types, accepted.
    EXPECT_EQ(attributeOf("59", "Parachute", "LineMaterial", "Type"), "BULK");
    EXPECT_EQ(attributeOf("60", "Parachute", "Material", "Type"), "BULK");
    // 61, 62: THROWS NullPointerException, this.shape is null.
    EXPECT_EQ(shapeOf("61"), std::nullopt);
    EXPECT_EQ(shapeOf("62"), std::nullopt);
    // 63 to 67: the required elements that are missing (63 reads: Shape=Ogive, Mass=0.01).
    EXPECT_EQ(shapeOf("63"), kOgive);
    EXPECT_EQ(numberOf("63", "NoseCone", "Mass"), 0.01);
    EXPECT_FALSE(hasLeaf("63", "NoseCone", "ShoulderDiameter"));
    EXPECT_FALSE(hasLeaf("64", "Transition", "ForeShoulderDiameter"));
    EXPECT_TRUE(hasLeaf("64", "Transition", "AftShoulderDiameter"));
    EXPECT_FALSE(hasLeaf("65", "Streamer", "Thickness"));
    EXPECT_FALSE(hasLeaf("66", "RailButton", "NutMass"));
    EXPECT_EQ(numberOf("66", "RailButton", "ScrewMass"), 0.001);
    EXPECT_FALSE(hasLeaf("67", "BodyTube", "InsideDiameter"));
    EXPECT_FALSE(hasLeaf("67", "BodyTube", "OutsideDiameter"));
    // 68: the order of the elements does not matter.
    EXPECT_EQ(lengthOf("68"), 18.0);
    EXPECT_EQ(tubeStringOf("68", "Manufacturer"), "Estes");
}

TEST(OrcEdgeCases, Batch1TextAndNames)
{
    // 69: Length=0.4699 (18.5 in): a CDATA section and a comment within the number.
    EXPECT_EQ(lengthOf("69"), 18.5);
    // 70: Length=18.0, in the default unit m: q:Unit is no Unit.
    EXPECT_EQ(lengthUnitOf("70"), std::nullopt);
    // 71: Length=0.4572: <q:Length> is another namespace's element.
    EXPECT_EQ(lengthOf("71"), 18.0);
    // 72: OpenRocket expands the entity (Length=0.4572); XmlScanner refuses a DTD that declares
    // one (QtRocket's deviation).
    EXPECT_EQ(failureOf("72").value_or("").substr(0, 20), "UNSUPPORTED_FORMAT: ");
    // 73: Manufacturer=Estes: the text keeps its spaces, which Manufacturer.getManufacturer()
    // trims.
    EXPECT_EQ(tubeStringOf("73", "Manufacturer"), "  Estes  ");
    // 74: OK 2 presets.
    EXPECT_EQ(componentCount("74", "BodyTube"), 2U);
    // 75, 76: the Thumbnail's text (75: Image=bytes5:68656c6c6f; 76: no Image).
    EXPECT_EQ(tubeStringOf("75", "Thumbnail"), "aGVsbG8=");
    EXPECT_EQ(tubeStringOf("76", "Thumbnail"), "!!!");
    // 77: an attribute of the root is ignored.
    EXPECT_TRUE(hasOrcRoot("77"));
    // 78, 82: the IOException of a document the parser refuses ("Content is not allowed in
    // prolog.", OpenRocket's log): a byte order mark read as a character, UTF-16 read as UTF-8.
    EXPECT_EQ(failureOf("78"), "PARSE: Content is not allowed in prolog.");
    EXPECT_EQ(failureOf("82"), "PARSE: Content is not allowed in prolog.");
    // 79: Length=-0.4572; 80: Length=Infinity.
    EXPECT_EQ(lengthOf("79"), -18.0);
    EXPECT_EQ(lengthOf("80"), kInf);
    // 81: Description=caf\uFFFD: the byte E9 is no UTF-8.
    EXPECT_EQ(tubeStringOf("81", "Description"), "caf\uFFFD");
}

TEST(OrcEdgeCases, Batch1Numbers)
{
    // 07, 08: Length=0.0 (rejected); 09 to 11: 0.4572; 12: NaN; 13: Infinity; 14, 15: 0.0;
    // 16: 0.4572; 17: 0.0; 18: 0.0127 (0.5 in); 19: -0.0.
    EXPECT_EQ(lengthOf("07"), std::nullopt);
    EXPECT_EQ(lengthOf("08"), std::nullopt);
    EXPECT_EQ(lengthOf("09"), 18.0);
    EXPECT_EQ(lengthOf("10"), 18.0);
    EXPECT_EQ(lengthOf("11"), 18.0);
    EXPECT_TRUE(isNaN(lengthOf("12")));
    EXPECT_EQ(lengthOf("13"), kInf);
    EXPECT_EQ(lengthOf("14"), std::nullopt);
    EXPECT_EQ(lengthOf("15"), std::nullopt);
    EXPECT_EQ(lengthOf("16"), 18.0);
    EXPECT_EQ(lengthOf("17"), std::nullopt);
    EXPECT_EQ(lengthOf("18"), 0.5);
    EXPECT_TRUE(isNegativeZero(lengthOf("19")));
}

TEST(OrcEdgeCases, Batch2Doubles)
{
    // N00 to N32: <Length Unit="m">, the probe's Length the number itself (0.0 where rejected).
    EXPECT_EQ(lengthOf("N00"), 18.0);
    EXPECT_EQ(lengthOf("N01"), 18.0);
    EXPECT_EQ(lengthOf("N02"), 18.0);
    EXPECT_EQ(lengthOf("N03"), 18.0);
    EXPECT_EQ(lengthOf("N04"), 18.0);
    EXPECT_EQ(lengthOf("N05"), std::nullopt);
    EXPECT_EQ(lengthOf("N06"), std::nullopt);
    EXPECT_EQ(lengthOf("N07"), 18.0);
    EXPECT_EQ(lengthOf("N08"), 18.0);
    EXPECT_EQ(lengthOf("N09"), std::nullopt);
    EXPECT_EQ(lengthOf("N10"), std::nullopt);
    EXPECT_EQ(lengthOf("N11"), std::nullopt);
    EXPECT_EQ(lengthOf("N12"), std::nullopt);
    EXPECT_EQ(lengthOf("N13"), std::nullopt);
    EXPECT_EQ(lengthOf("N14"), std::nullopt);
    EXPECT_EQ(lengthOf("N15"), -0.5);
    EXPECT_EQ(lengthOf("N16"), std::nullopt);
    EXPECT_EQ(lengthOf("N17"), std::nullopt);
    EXPECT_EQ(lengthOf("N18"), std::nullopt);
    EXPECT_EQ(lengthOf("N19"), std::nullopt);
    EXPECT_EQ(lengthOf("N20"), std::nullopt);
    EXPECT_EQ(lengthOf("N21"), std::nullopt);
    EXPECT_EQ(lengthOf("N22"), 18.0);
    EXPECT_EQ(lengthOf("N23"), -kInf);
    EXPECT_EQ(lengthOf("N24"), kInf);
    EXPECT_TRUE(isNaN(lengthOf("N25")));
    EXPECT_EQ(lengthOf("N26"), std::nullopt);
    EXPECT_EQ(lengthOf("N27"), 0.0);  // read, as a zero: Length=0.0
    EXPECT_EQ(lengthOf("N28"), 4.9e-324);
    EXPECT_EQ(lengthOf("N29"), 2.225073858507201e-308);
    EXPECT_EQ(lengthOf("N30"), 9007199254740992.0);
    EXPECT_EQ(lengthOf("N31"), 0.1);
    EXPECT_EQ(lengthOf("N32"), kInf);
}

TEST(OrcEdgeCases, Batch2Integers)
{
    // I00 to I15: Sides=6, 6, 6, -6, none, 12, -5, -5, -2147483648, 1215752191, 0, none, none,
    // none, 0, 0.
    EXPECT_EQ(sidesOf("I00"), 6);
    EXPECT_EQ(sidesOf("I01"), 6);
    EXPECT_EQ(sidesOf("I02"), 6);
    EXPECT_EQ(sidesOf("I03"), -6);
    EXPECT_EQ(sidesOf("I04"), std::nullopt);
    EXPECT_EQ(sidesOf("I05"), 12);
    EXPECT_EQ(sidesOf("I06"), -5);
    EXPECT_EQ(sidesOf("I07"), -5);
    EXPECT_EQ(sidesOf("I08"), std::numeric_limits<int>::min());
    EXPECT_EQ(sidesOf("I09"), 1215752191);
    EXPECT_EQ(sidesOf("I10"), 0);
    EXPECT_EQ(sidesOf("I11"), std::nullopt);
    EXPECT_EQ(sidesOf("I12"), std::nullopt);
    EXPECT_EQ(sidesOf("I13"), std::nullopt);
    EXPECT_EQ(sidesOf("I14"), 0);
    EXPECT_EQ(sidesOf("I15"), 0);
}

TEST(OrcEdgeCases, Batch2Booleans)
{
    // B00 to B19: Filled=true, false, true, false, true, true, then none for B06 to B13,
    // false for B14, none for B15 to B18, true for B19.
    EXPECT_EQ(filledOf("B00"), true);
    EXPECT_EQ(filledOf("B01"), false);
    EXPECT_EQ(filledOf("B02"), true);
    EXPECT_EQ(filledOf("B03"), false);
    EXPECT_EQ(filledOf("B04"), true);
    EXPECT_EQ(filledOf("B05"), true);
    EXPECT_EQ(filledOf("B06"), std::nullopt);
    EXPECT_EQ(filledOf("B07"), std::nullopt);
    EXPECT_EQ(filledOf("B08"), std::nullopt);
    EXPECT_EQ(filledOf("B09"), std::nullopt);
    EXPECT_EQ(filledOf("B10"), std::nullopt);
    EXPECT_EQ(filledOf("B11"), std::nullopt);
    EXPECT_EQ(filledOf("B12"), std::nullopt);
    EXPECT_EQ(filledOf("B13"), std::nullopt);
    EXPECT_EQ(filledOf("B14"), false);
    EXPECT_EQ(filledOf("B15"), std::nullopt);
    EXPECT_EQ(filledOf("B16"), std::nullopt);
    EXPECT_EQ(filledOf("B17"), std::nullopt);
    EXPECT_EQ(filledOf("B18"), std::nullopt);
    EXPECT_EQ(filledOf("B19"), true);
}

TEST(OrcEdgeCases, Batch2Shapes)
{
    // S00: Shape=Ogive; S01 to S04: THROWS NullPointerException (this.shape is null); S05:
    // Haack series; S06: Power series.
    EXPECT_EQ(shapeOf("S00"), kOgive);
    EXPECT_EQ(shapeOf("S01"), std::nullopt);
    EXPECT_EQ(shapeOf("S02"), std::nullopt);
    EXPECT_EQ(shapeOf("S03"), std::nullopt);
    EXPECT_EQ(shapeOf("S04"), std::nullopt);
    EXPECT_EQ(shapeOf("S05"), kHaack);
    EXPECT_EQ(shapeOf("S06"), kPower);
}

TEST(OrcEdgeCases, Batch2Text)
{
    // T01: Length=0.2032 (8 in): the text after the child element.
    EXPECT_EQ(lengthOf("T01"), 8.0);
    // T02, T03: Description= and Description= a&b\nc\t.
    EXPECT_EQ(tubeStringOf("T02", "Description"), "");
    EXPECT_EQ(tubeStringOf("T03", "Description"), " a&b\nc\t");
    // T04: THROWS IllegalArgumentException: Invalid material type  BULK specified for Component.
    EXPECT_EQ(attributeOf("T04", "BodyTube", "Material", "Type"), " BULK");
    // T05: Mass=0.226796185 (0.5 lb); T06: THROWS IllegalArgumentException: name=kg.
    EXPECT_EQ(numberOf("T05", "BodyTube", "Mass"), 0.5);
    EXPECT_EQ(attributeOf("T05", "BodyTube", "Mass", "Unit"), "lb");
    EXPECT_EQ(lengthUnitOf("T06"), "kg");
    // T07: Material=[Bulk 'Paper test' 0.0 ...]: the second Materials list holds only Other.
    EXPECT_EQ(sectionStringOf("T07", "Name"), "Other");
    EXPECT_EQ(tubeStringOf("T07", "Material"), "Paper test");
    // T08: Legacy=true for <Legacy/>.
    EXPECT_TRUE(hasWrapper("T08", "Legacy"));
    // T09: the stray text between components is no component.
    EXPECT_EQ(componentCount("T09", "BodyTube"), 1U);
    // T10: Material=[Bulk 'board' 0.0 Other user].
    EXPECT_EQ(tubeStringOf("T10", "Material"), "board");
    // T11, T12: Manufacturer= and PartNo= (empty, not missing).
    EXPECT_EQ(tubeStringOf("T11", "Manufacturer"), "");
    EXPECT_EQ(tubeStringOf("T12", "PartNumber"), "");
    // T13: the IOException of g/m2 for a BULK material.
    EXPECT_EQ(sectionUomOf("T13"), "g/m2");
    // T14: THROWS NullPointerException: " BULK " is no MaterialTypeDTO.
    EXPECT_EQ(QtRocket::JaxbLexical::parseEnum(sectionStringOf("T14", "Type").value_or("?"),
                                               kMaterialTypes),
              std::nullopt);
    // T15: Material=[Bulk 'Cardboard' 680.0 shear 1.0E9 Other user].
    EXPECT_EQ(shearModulusOf("T15"), 1e9);
    // T16: the unknown attribute Foo is ignored.
    EXPECT_EQ(lengthUnitOf("T16"), "in");
    EXPECT_EQ(attributeOf("T16", "BodyTube", "Length", "Foo"), "bar");
    // T17: OK 0 presets, the body tube is in the namespace urn:x.
    EXPECT_EQ(componentCount("T17", "BodyTube"), 0U);
    // T18: Length=0.4572 (xmlns="" is no namespace); T19: THROWS NullPointerException, this.length
    // is null (the Length is urn:x's).
    EXPECT_EQ(lengthOf("T18"), 18.0);
    EXPECT_FALSE(hasLeaf("T19", "BodyTube", "Length"));
    // T20: a Version with a child element is still never read.
    EXPECT_EQ(lengthOf("T20"), 18.0);
}

TEST(OrcEdgeCases, Batch3Doubles)
{
    // XN00 to XN12: Length=0.0, 0.0, 0.0, Infinity, 18.0, 0.0, 0.5, 0.0, -3.0, 0.0,
    // 0.0, 18.0, 18.0.
    EXPECT_EQ(lengthOf("XN00"), std::nullopt);
    EXPECT_EQ(lengthOf("XN01"), std::nullopt);
    EXPECT_EQ(lengthOf("XN02"), std::nullopt);
    EXPECT_EQ(lengthOf("XN03"), kInf);
    EXPECT_EQ(lengthOf("XN04"), 18.0);
    EXPECT_EQ(lengthOf("XN05"), std::nullopt);
    EXPECT_EQ(lengthOf("XN06"), 0.5);
    EXPECT_EQ(lengthOf("XN07"), std::nullopt);
    EXPECT_EQ(lengthOf("XN08"), -3.0);
    EXPECT_EQ(lengthOf("XN09"), std::nullopt);
    EXPECT_EQ(lengthOf("XN10"), std::nullopt);
    EXPECT_EQ(lengthOf("XN11"), 18.0);  // "18" read, then "x" rejected
    EXPECT_EQ(lengthOf("XN12"), 18.0);
    // XT26, XT27: Length=0.0, a control character of XML 1.1 is no white space.
    EXPECT_EQ(lengthOf("XT26"), std::nullopt);
    EXPECT_EQ(lengthOf("XT27"), std::nullopt);
}

TEST(OrcEdgeCases, Batch3Integers)
{
    // XI00 to XI17: Sides=-6, -6, 6, 6, 6, 0, 6, 2147483647, 0, -6, 6, none, 6, none, 0, 6, 6,
    // none.
    EXPECT_EQ(sidesOf("XI00"), -6);
    EXPECT_EQ(sidesOf("XI01"), -6);
    EXPECT_EQ(sidesOf("XI02"), 6);
    EXPECT_EQ(sidesOf("XI03"), 6);
    EXPECT_EQ(sidesOf("XI04"), 6);
    EXPECT_EQ(sidesOf("XI05"), 0);
    EXPECT_EQ(sidesOf("XI06"), 6);
    EXPECT_EQ(sidesOf("XI07"), std::numeric_limits<int>::max());
    EXPECT_EQ(sidesOf("XI08"), 0);
    EXPECT_EQ(sidesOf("XI09"), -6);
    EXPECT_EQ(sidesOf("XI10"), 6);
    EXPECT_EQ(sidesOf("XI11"), std::nullopt);
    EXPECT_EQ(sidesOf("XI12"), 6);             // "6" read, then "x" rejected
    EXPECT_EQ(sidesOf("XI13"), std::nullopt);  // the empty text before <b/> is not handed over
    EXPECT_EQ(sidesOf("XI14"), 0);             // the empty text after <b/> is
    EXPECT_EQ(sidesOf("XI15"), 6);
    EXPECT_EQ(sidesOf("XI16"), 6);
    EXPECT_EQ(sidesOf("XI17"), std::nullopt);  // nor white space before <b/>
    // XT28: no Sides, a control character of XML 1.1 rejects the text.
    EXPECT_EQ(sidesOf("XT28"), std::nullopt);
}

TEST(OrcEdgeCases, Batch3Booleans)
{
    // XB00 to XB35: Filled=false for XB00 to XB05, true, true, false, none, true, false, none,
    // none, none, false, none, false, none, true, true, none for XB21 to XB25, true, none for
    // XB27 to XB30, true, none, false, none, false.
    EXPECT_EQ(filledOf("XB00"), false);
    EXPECT_EQ(filledOf("XB01"), false);
    EXPECT_EQ(filledOf("XB02"), false);
    EXPECT_EQ(filledOf("XB03"), false);
    EXPECT_EQ(filledOf("XB04"), false);
    EXPECT_EQ(filledOf("XB05"), false);
    EXPECT_EQ(filledOf("XB06"), true);
    EXPECT_EQ(filledOf("XB07"), true);
    EXPECT_EQ(filledOf("XB08"), false);
    EXPECT_EQ(filledOf("XB09"), std::nullopt);
    EXPECT_EQ(filledOf("XB10"), true);
    EXPECT_EQ(filledOf("XB11"), false);
    EXPECT_EQ(filledOf("XB12"), std::nullopt);
    EXPECT_EQ(filledOf("XB13"), std::nullopt);
    EXPECT_EQ(filledOf("XB14"), std::nullopt);
    EXPECT_EQ(filledOf("XB15"), false);
    EXPECT_EQ(filledOf("XB16"), std::nullopt);
    EXPECT_EQ(filledOf("XB17"), false);
    EXPECT_EQ(filledOf("XB18"), std::nullopt);
    EXPECT_EQ(filledOf("XB19"), true);
    EXPECT_EQ(filledOf("XB20"), true);  // "t" alone is rejected: true stays
    EXPECT_EQ(filledOf("XB21"), std::nullopt);
    EXPECT_EQ(filledOf("XB22"), std::nullopt);
    EXPECT_EQ(filledOf("XB23"), std::nullopt);
    EXPECT_EQ(filledOf("XB24"), std::nullopt);
    EXPECT_EQ(filledOf("XB25"), std::nullopt);
    EXPECT_EQ(filledOf("XB26"), true);
    EXPECT_EQ(filledOf("XB27"), std::nullopt);
    EXPECT_EQ(filledOf("XB28"), std::nullopt);
    EXPECT_EQ(filledOf("XB29"), std::nullopt);
    EXPECT_EQ(filledOf("XB30"), std::nullopt);
    EXPECT_EQ(filledOf("XB31"), true);
    EXPECT_EQ(filledOf("XB32"), std::nullopt);
    EXPECT_EQ(filledOf("XB33"), false);
    EXPECT_EQ(filledOf("XB34"), std::nullopt);
    EXPECT_EQ(filledOf("XB35"), false);
    // XT29: no Filled, a control character of XML 1.1 is false, and the 1 after it null.
    EXPECT_EQ(filledOf("XT29"), std::nullopt);
}

TEST(OrcEdgeCases, Batch3Shapes)
{
    // XS00 to XS02, XS05: Shape=Ogive; XS03, XS04, XS06: THROWS NullPointerException.
    EXPECT_EQ(shapeOf("XS00"), kOgive);
    EXPECT_EQ(shapeOf("XS01"), kOgive);
    EXPECT_EQ(shapeOf("XS02"), kOgive);
    EXPECT_EQ(shapeOf("XS03"), std::nullopt);  // the empty text after <b/> is the last
    EXPECT_EQ(shapeOf("XS04"), std::nullopt);
    EXPECT_EQ(shapeOf("XS05"), kOgive);
    EXPECT_EQ(shapeOf("XS06"), std::nullopt);
}

TEST(OrcEdgeCases, Batch3Text)
{
    // XT01 to XT04: Length=0.2032 (8 in), then 0.4572 (18 in) three times.
    EXPECT_EQ(lengthOf("XT01"), 8.0);
    EXPECT_EQ(lengthOf("XT02"), 18.0);
    EXPECT_EQ(lengthOf("XT03"), 18.0);
    EXPECT_EQ(lengthOf("XT04"), 18.0);
    // XT05 to XT08: Description=, x<y, a b, two rockets.
    EXPECT_EQ(tubeStringOf("XT05", "Description"), "");
    EXPECT_EQ(tubeStringOf("XT06", "Description"), "x<y");
    EXPECT_EQ(tubeStringOf("XT07", "Description"), "a b");
    EXPECT_EQ(tubeStringOf("XT08", "Description"), "\U0001F680\U0001F680");
    // XT09, XT10: xsi:nil is ignored (Length=0.4572, Description=d).
    EXPECT_EQ(lengthOf("XT09"), 18.0);
    EXPECT_EQ(tubeStringOf("XT10", "Description"), "d");
    // XT11 to XT15: the attribute values (XT12: THROWS IllegalArgumentException: name=in ;
    // XT13: name=in\t; XT14: Length=0.18, the unit cm).
    EXPECT_EQ(lengthUnitOf("XT11"), "in");
    EXPECT_EQ(lengthUnitOf("XT12"), "in ");
    EXPECT_EQ(lengthUnitOf("XT13"), "in\t");
    EXPECT_EQ(lengthUnitOf("XT14"), "cm");
    EXPECT_EQ(attributeOf("XT15", "BodyTube", "Material", "Type"), "BULK");
    // XT16: PartNo=BT-X \t.
    EXPECT_EQ(tubeStringOf("XT16", "PartNumber"), "BT-X \t");
    // XT25, XT45 to XT48: Description=, x, a space, nothing, a\rb\r\nc\nd.
    EXPECT_EQ(tubeStringOf("XT25", "Description"), "");
    EXPECT_EQ(tubeStringOf("XT45", "Description"), "x");
    EXPECT_EQ(tubeStringOf("XT46", "Description"), " ");
    EXPECT_EQ(tubeStringOf("XT47", "Description"), "");
    EXPECT_EQ(tubeStringOf("XT48", "Description"), "a\rb\r\nc\nd");
}

TEST(OrcEdgeCases, Batch3Names)
{
    // XT17: Legacy=false, the Legacy is urn:x's; XT18: Material=[Bulk 'Paper test' 0.0 ...], the
    // Materials are urn:x's; XT19: an empty default namespace is none; XT20: the IOException of
    // a root of urn:p; XT21: OK 0 presets, the empty second wrapper; XT22: THROWS
    // NullPointerException, no Manufacturer; XT23: OK 0 presets; XT24: OK 1 presets.
    EXPECT_FALSE(hasWrapper("XT17", "Legacy"));
    EXPECT_FALSE(hasWrapper("XT18", "Materials"));
    EXPECT_TRUE(hasOrcRoot("XT19"));
    EXPECT_EQ(failureOf("XT20"), std::nullopt);
    EXPECT_FALSE(hasOrcRoot("XT20"));
    EXPECT_TRUE(hasWrapper("XT21", "Components"));
    EXPECT_EQ(componentCount("XT21", "BodyTube"), 0U);
    EXPECT_EQ(componentCount("XT22", "BodyTube"), 1U);
    EXPECT_EQ(tubeStringOf("XT22", "Manufacturer"), std::nullopt);
    EXPECT_TRUE(hasOrcRoot("XT23"));
    EXPECT_FALSE(hasWrapper("XT23", "Components"));
    EXPECT_TRUE(hasOrcRoot("XT24"));
    EXPECT_EQ(lengthOf("XT24"), 18.0);
}

TEST(OrcEdgeCases, Batch3Repetitions)
{
    // XT30: Length=0.0, the last Length's text rejected; XT44: 0.4572.
    EXPECT_EQ(lengthOf("XT30"), std::nullopt);
    EXPECT_EQ(lengthOf("XT44"), 18.0);
    // XT31, XT32: Sides=6 (the second rejected), Sides=0 (the second empty).
    EXPECT_EQ(sidesOf("XT31"), 6);
    EXPECT_EQ(sidesOf("XT32"), 0);
    // XT33, XT39: 'Cardboard' 0.0 (the primitive is the last element's); XT38, XT40: 680.0.
    EXPECT_EQ(densityOf("XT33"), std::nullopt);
    EXPECT_EQ(densityOf("XT39"), std::nullopt);
    EXPECT_EQ(densityOf("XT38"), 680.0);
    EXPECT_EQ(densityOf("XT40"), 680.0);
    // XT34: Filled=true (the second, "t", rejected); XT35: Description=; XT36: THROWS
    // NullPointerException (BLUNT is null).
    EXPECT_EQ(filledOf("XT34"), true);
    EXPECT_EQ(tubeStringOf("XT35", "Description"), "");
    EXPECT_EQ(shapeOf("XT36"), std::nullopt);
    // XT37, XT41, XT42: shear 1.0E9 (the Double keeps it); XT43: none (the builtin 4.0E8).
    EXPECT_EQ(shearModulusOf("XT37"), 1e9);
    EXPECT_EQ(shearModulusOf("XT41"), 1e9);
    EXPECT_EQ(shearModulusOf("XT42"), 1e9);
    EXPECT_EQ(shearModulusOf("XT43"), std::nullopt);
}

TEST(OrcEdgeCases, Batch3Thumbnails)
{
    // XH00 to XH36: the Thumbnail's text, untouched (OpenRocketComponentLoader decodes it).
    const std::vector<std::string_view> texts{
        "aGVsbG8",   "aGVs bG8=", "aGVsbG8=\n", "",       "QQ==",      "QQ",       "Q",
        "aGV!sbG8=", "====",      "QUJD",       " QUJD ", "QUJD=",     "QU=JD",    "4pyI",
        "QU==",      "QUI=",      "QUJDQQ==",   "Q===",   "=QUJ",      "QU=D",     "QQ==QUJD",
        "QUJD====",  "AAAA",      "////",       "++++",   "-_-_",      "QU!D",     "QU D",
        "QQ=",       "QQ=A",      "A===",       "AA==",   "QUJ\u00E9", "QUJDQUJD", "QUJD\tQUJD",
        "QR==",      "QUK="};
    for (std::size_t i = 0; i < texts.size(); i++)
    {
        const std::string id = std::format("XH{:02}", i);
        EXPECT_EQ(tubeStringOf(id, "Thumbnail"), texts[i]) << id;
    }
}

}  // namespace

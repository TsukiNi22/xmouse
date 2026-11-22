/*
 * Check of the member core of the android app on a PC (scripts/check_android_core.sh): the same Java code, its
 * identity from a PKCS12 file, against a real xmouse
 *  join <link> <keystore>   the "phone" joins the network of the link (the device accepts it)
 *  host <keystore>          the "phone" creates a network, prints its link, accepts the device that joins, then its
 *                           cursor goes on the screen of that device and clicks there
*/
import io.github.tsukini22.xmouse.core.Network;
import io.github.tsukini22.xmouse.core.Node;

import java.io.FileInputStream;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.cert.Certificate;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;

public class NodeCheck {
    static final class Memory implements Network.Store {
        String text = "";
        public String load() {return this.text;}
        public void save(String json) {this.text = json;}
    }

    public static void main(String[] args) throws Exception {
        String mode = args[0];
        String keystore = mode.equals("join") ? args[2] : args[1];
        KeyStore ks = KeyStore.getInstance("PKCS12");
        try (FileInputStream in = new FileInputStream(keystore)) {ks.load(in, "checks".toCharArray());}
        KeyManagerFactory kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
        kmf.init(ks, "checks".toCharArray());
        SSLContext server = SSLContext.getInstance("TLS");
        server.init(kmf.getKeyManagers(), null, null);
        Certificate cert = ks.getCertificate(ks.aliases().nextElement());
        byte[] fp = MessageDigest.getInstance("SHA-256").digest(cert.getEncoded());

        CountDownLatch joined = new CountDownLatch(1);
        AtomicReference<String> result = new AtomicReference<>("");
        Node node = new Node(new Memory(), new Memory(), server, fp, "test-phone", "1.0.0");
        node.start(mode.equals("join") ? 47960 : 47961, new Node.Listener() {
            public void changed() {}
            public void joinRequest(String pending, String name, String os) {
                System.out.println("JOIN-REQUEST " + name + " (" + os + "): accepted");
                node.approve(pending, true);
            }
            public void joinResult(boolean ok, String message) {result.set((ok ? "OK " : "FAIL ") + message); joined.countDown();}
            public void notice(String text) {System.out.println("NOTICE " + text);}
            public void screen(Map<String, Object> event) {System.out.println("SCREEN " + event);}
            public void map(Map<String, Object> map) {}
            public void clipboard(String text) {System.out.println("CLIP " + text);}
            public void link(String from, String url) {System.out.println("LINK-FROM " + from + " " + url);}
        });
        Thread.sleep(1500);
        if (mode.equals("join")) {
            node.join(args[1], "");
            if (!joined.await(60, TimeUnit.SECONDS)) {System.out.println("JOIN timeout"); System.exit(1);}
            System.out.println("JOIN " + result.get());
            Thread.sleep(4000);
        } else {
            node.createNetwork("phone net");
            Thread.sleep(3000); // the code key (argon2id) is computed
            System.out.println("LINK " + node.joinLink().replace(node.joinLink().split("/")[2].split(":")[0], "127.0.0.1"));
            System.out.flush();
            long end = System.currentTimeMillis() + 90000;
            while (System.currentTimeMillis() < end && node.members().stream().noneMatch(m -> !m.self && m.online)) Thread.sleep(500);
            Thread.sleep(2500); // the device publishes its screens
            for (int i = 0; i < 20; i++) {node.move(5, 3); Thread.sleep(20);}
            node.button(0, true);
            node.button(0, false);
            node.text("hi");
            Thread.sleep(1500);
        }
        for (Node.Member m: node.members()) System.out.println("MEMBER " + m.name + " " + m.kind + (m.self ? " self" : (m.online ? " online" : " offline")));
        node.stop();
        System.exit(0);
    }
}

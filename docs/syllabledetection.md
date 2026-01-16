Syllable Detection
音声の音楽的特徴を分析するためには、基本的な単位として音節を検出することが重要です。音節は通常、母音を中心に構成されるため、音声信号から母音を検出する必要があります。既存の音声分析ソフトウェア（例：Praat）はオフラインでの音節検出機能を備えていますが、リアルタイムでの処理が求められる場合は独自の手法を開発する必要があります。

音節検出の手法と課題
　基本的なエンベロープ追跡とピーク検出:
　　音声信号に対して300～900Hzのバンドパスフィルタを適用し、エンベロープを取得、その後、微分器を用いてピークを検出します。しかし、この方法では無声音（例：'t'、's'、'th'）や有声音の子音（例：'n'、'l'）でもピークが検出され、誤検出が多くなります。
　メル周波数分析と周期性の利用:
　　人間の聴覚特性に近いメル尺度を用いてスペクトルを分析し、中周波数帯域（約500～3200Hz）に焦点を当てます。さらに、ピッチ検出アルゴリズム（例：YIN）を使用して周期性を測定し、無声音を除外します。しかし、ピッチ検出には遅延が伴い、低音域や急激な音声変化に対しては精度が低下することがあります。
　マルチバンドの有声/無声マスク:
　　検出された基本周波数に基づいて有声音のみを再合成し、元のスペクトルと比較することで、各周波数帯域が有声音か無声音かを判別します。その後、等ラウドネス曲線（ISO 226:2003）を適用して、人間の聴覚に基づくエンベロープを生成します。この方法は精度が高いものの、処理が複雑であり、急激な音声変化には対応が難しい場合があります。
　メル周波数ケプストラム係数（MFCC）によるゲーティング:
　　第2ケプストラム係数が有声音のエネルギーと相関することを利用し、これを中周波数帯域のエンベロープのゲートとして使用します。この方法は環境や録音条件に敏感で、複数の閾値設定が必要となるため、リアルタイム処理には適していない場合があります。
　ゼロ周波数フィルタリングによる声門活動の検出:
　　声門からのパルスはゼロ周波数成分を含むため、ゼロ周波数共振器を用いてこれらのパルスを検出します。この手法により、有声音と無声音を効果的に分離できます。得られた声門活動の指標を用いて、中周波数帯域のエンベロープをゲーティングすることで、リアルタイムで安定した音節検出が可能となります。
最も効果的なリアルタイム音節検出手法は、声門活動の指標とバンド制限された振幅エンベロープを組み合わせることで、母音を中心とした音節を正確に捉え、無声音やノイズを抑制することができます。

音節の指導は日本語話者が英語を学ぶにあたってどんな良さがありますか？
　正確な発音の習得
　　英語は、音節ごとにアクセント（強勢）を置くことでリズムや意味を伝える言語です。音節の切り分けや強弱のパターンを理解することで、正しい発音や自然なイントネーションが身につきやすくなります。
　リズム感とイントネーションの向上
 　音節を意識して発音練習を行うと、英語特有のリズムやメロディーが体に染み込み、聞き取りやスピーキング時の流暢さが向上します。日本語は比較的均一な拍（モーラ）で構成されるのに対し、英語は強弱のある音節が連続するため、この違いを意識することが大切です。
 リスニングスキルの強化
 　単語や文章の中で、どこにアクセントがあるか、どこで音節が切れているかを理解することは、聞き取りの際に重要な手がかりとなります。音節に着目することで、ネイティブの発話のリズムや強弱を把握しやすくなり、リスニングの精度が上がります。
 スペリングと発音の関連理解
　 英語では、音節単位での発音と綴りの対応関係がある場合が多いです。音節の構造や特徴を学ぶことで、単語のスペルや発音の規則性に気づき、語彙習得や正書法の理解にも役立ちます。



When I started to look at musical traits of speech and how I could go about measuring and analyzing them, I soon realized I needed to be able to quantize some kind of fundamental unit from which I could extract basic information like pitch and speech rate etc. Even though linguists  subdivide into speech into phonemes, the smallest unit that makes sense musically is the syllable which is more akin to the concept of a note in music. That syllables also makes deep sense as a fundamental unit in speech can be seen by the fact that a number of written languages have signs for syllables rather than phonemes.

Syllables are usually centered around a vowel, so in order to segment speech into syllables I needed the ability to detect vowels in a speech stream. Existing speech analyzing software like Praat already have the ability to detect syllables through offline analyses, but since I needed to process speech and generate sound in real time, I had to make something myself. This brought me straight into some heavy technical considerations (for a musician at least), which never the less was interesting to figure out.

I set out to program a real time syllable detector by looking at already existing techniques, using the IRCAM developed FTM and Mubu libraries for the Max/MSP programming environment. The task is not as trivial as it may seem, as standard envelope following and onset detection used for acoustic instruments are not very reliable when it comes to speech – plosives and fricatives can be quite loud and carry a lot of acoustic energy but we do not perceive them as fundamental musical units like we do with the vowels. Syllable detection is however called for in many tasks like automatic speech to text transcription, and several techniques have been proposed (Mermelstein, 1975; Howitt, 2000; Prasanna, Reddy, & Krishnamoorthy, 2009) etc. Common for the different approaches is a focus on the mid frequency region where the vowels’ strongest formants are present (roughly 300-3000Hz). In addition, unvoiced phonemes need to be ditched (typically s, f, p, t, k) and one way to that is to segment into voiced and unvoiced regions by measuring periodicity. But at the same time voiced consonants also needs to be filtered out (z, v, g, b, d, m, n, l) and that can be a bit more difficult, but by discarding lower frequencies these can also be somewhat suppressed.

I tried to model several existing techniques to compare their performance and to understand how they worked. Many different sources of speech were used for testing, but for the sake of comparison I will use the same clip of a spoken sentence in the illustrations, with a female speaking the sentence “and at last the north wind gave up the attempt” in english.

[https://www.orchestraofspeech.com/wp-content/uploads/2014/05/audioenvelope.png]

The last ’t’ clearly present in the envelope is one of the elements we want to suppress, as well as the ’s’ in ‘last’ after the second syllable and ‘th’ in ‘north’ after the third.

I then applied a simple 5th order bandpass filter at 300-900 Hz to the source to only listen to the frequency region of the first formant, and used a simple differentiator to mark peaks in the resulting envelope.

[https://www.orchestraofspeech.com/wp-content/uploads/2014/05/bp300-900-1024x332.png]

The peaks are neat, and though some of the unvoiced phonemes are reduced somewhat there is still a detected peak for nearly every phoneme, including some loud ones (‘n‘ of word ‘wind’ in the 9th for instance). That led me to add some more features to the simple peak picking differentiator (which just measure the direction of envelope slope and mark a peak when transiting from increase to decrease). In addition to a level threshold I added a hold phase of 50 ms to avoid double triggers caused by level fluctuations within a vowel, and a requirement for the envelope to dip at least 3 dB below last peak before looking for the next peak. That did help filter out some of the soft unvoiced sounds, but the loud ones still remained.

I then turned to the frequency domain to better have a look at the sonic content of the different phonemes. In this context it makes most sense to look at the sound in a way which resembles how the ear perceive it, as this is not the same as computers do. Digitally, sound is usually represented in the frequency domain in a linear fashion with the short time Fourier Transform algorithm. But the ear senses loudness over a range of frequency bands that are not spaced linearly, and to model this the spectrum is usually expressed in a logarithmic scale like the bark or mel scales instead of the linear Hertz scale. For speech analysis, the mel scale is nice since the important formant region is so well represented.

[https://www.orchestraofspeech.com/wp-content/uploads/2014/05/loudnessjuxtaposed-1024x332.png]

As we can see, the bottom bands are quite heavy with the fundamental and strong lower partials, the middle bands varies distinctively with the different vowels and voiced consonants, and some high frequency sounds like ’s’ , ’t’ and ‘th’ can also clearly be seen at the top. To zoom in on the vowels, we can of course get rid of the top bands altogether to avoid the high frequencies of ’s’ and ’t’. As we can also observe that voiced consonants like ’l’ (between the first two syllables) and ‘n’ (at the end of ‘wind’ in the middle of the sentence) have all their energy concentrated in the bottom bands, we can also try to attenuate the lower bands completely.

The resulting envelope from averaging mel bands 5-16 (ca. 500-3200 Hz) is clearly more pointed than by simply using a bandpass filter for the 300-900 Hz region:

[https://scrapbox.io/files/679d891e17e122abd61d0209.png]

Peaks are still detected for some unvoiced sounds though. When tested with recordings of different speakers and languages, trigging blips on every peak detected, this solution actually sounded quite well, but too many false peaks of unvoiced sounds were detected. This led me further to look at the measure of periodicity to mask out the unvoiced sounds.

One type of widely used pitch detection algorithm like the “yin”-algorithm (de Cheveigné & Kawahara, 2002) available as modules in Max/MSP and the ftm/Gabor library, works by detecting periodic repetitions in a stream of sound in order to estimate a probable fundamental frequency. To detect a repetition, this operation requires a buffer of at least twice the length of the fundamental. That means that in order to detect low pitches down to say 50 Hz, this will introduce a delay of at least 40 ms (as one period of 50 Hz is 20 ms long). When designing a syllable detector for real time use one aim is to keep the latency at a minimum, but depending on how much better it will perform this still might be worth it.

I started out by using the periodicity measure simply as a multiplier for the mid mel band level envelope used above, along with a a pre-emphasis (high pass) filter with a factor of 0.97 used in speech processing to flatten the spectrum and represent formants as equally loud. The quick changes between different frequency regions in speech results in a very noisy periodicity measure, so this did not go without errors.

[https://www.orchestraofspeech.com/wp-content/uploads/2014/05/mid-mel-band-energy-500-3200Hz-971x1024.png]
It reduced some unwanted sounds, and in the envelope below we can see that though there still are small peaks for some of the unvoiced sounds, we now have only vowels detected as peaks (some drawn together syllables like the first one (‘and at’, pronounced like one syllable ’n’at’) are now detected as one peak, but that might be the price to pay to reduce the false hits on unvoiced sounds.

I then based my further approach on an even more refined way of using the periodicity measure, described by Nicolas Obin in his “Syll-O-Matic”(Obin, Lamare, & Roebel, 2013). The algorithm is for offline analysis and includes several more stages, but at least one part appropriate for real time use is the way he looks to voiced regions to mask out the unvoiced sounds, proposing a multi band voiced/unvoiced mask approach where each mel band is determined voiced or unvoiced before summing the bands. To achieve this I used an ‘analysis by synthesis’ approach, by resynthesizing the harmonics of the detected fundamental and thus recreating a spectrum containing only the voiced parts. This spectrum is then compared to the original spectrum to determine how much of each band’s energy comes from the voiced harmonics, and is then summed and smoothed into an envelope.

[https://www.orchestraofspeech.com/wp-content/uploads/2014/05/syll-able-1024x1002.png]

When using the whole voiced spectrum like this it becomes important to take psychoacoustics into account, to get an envelope that resembles our perception of loudness, as this – like our perception of frequency – is not linear. In the implementation above the specific loudness is calculated like Obin proposes by using an exponent of 0.23, but this is only an approximation as the exponent has been shown to be different for the cochlear bands of the ear (Fletcher, 1933; Robinson & Dadson, 1956; Zwicker & Fastl 1999).

To make a more detailed perceptual modeling of loudness I made a multi band scaling filter where the level of each mel band is scaled according to the upper and lower hearing thresholds for each critical band as described in the updated ISO 226:2003 standard detailing the so-called “equal loudness curve”, which aims to reflect how loud the different frequencies actually have to be in order for us to perceive them to be equally loud. With this, the lower and upper bands are attenuated in about the same way as the ear does.
[https://www.orchestraofspeech.com/wp-content/uploads/2014/05/equalloudnessfilter-1024x486.png]
In the comparison below, we can clearly see how the masked envelope (bottom) finally lacks the unvoiced sounds altogether, and the peaks (represented by the grey bars at the bottom) only appear at vowels.
[https://www.orchestraofspeech.com/wp-content/uploads/2014/05/vuvcomparison-1024x320.png]
This solution performed really well when tested with a range of speech recordings, but can sometimes fail at very low creaky pitches typically at the end of sentences where the pitch detector cannot estimate a fundamental properly. Another weakness was stuttering or dropouts due to the fast changing sounds of speech, also causing sporadic errors in the pitch detection algorithm feeding the resynthesis stage.

Though I was pleased with the performance of this syllable detector, I also developed a solution that I have not seen described anywhere but which at first looked promising and also simpler to implement. In speech-to-text analysis, a process called the mel frequency cepstrum is used extensively for analyzing and recognizing different phonemes. In this process the non-linear but perceptually more correct mel frequency spectrum described above is transformed into a spectrum of a spectrum, into a this kind of imaginary domain which with a humorous anagram is dubbed the ‘cepstrum’ (Bogert, Healy, & Tukey, 1963) In short, the cepstrum describes the shape of the spectrum, and when looking at the lower cepstrum bands (mel frequency cepstrum coefficients, or mfcc’s for short), there is clearly a correlation between voiced sounds and the second coefficient. So by simply having the second mfcc (with an appropriate threshold) act as a gate for the loudness envelope of the formant region, we could have a nice sturdy syllable detector.

[https://www.orchestraofspeech.com/wp-content/uploads/2014/05/mfccgate+formantband-1024x288.png]

The loud (red) regions of the second mfcc can clearly be seen to correlate with the presence of voiced speech sounds. This solution also worked particularly well for the problematic creaky low last syllables mentioned above, and was also faster and less prone to fluctuations than the periodicity (pitch detection) measure. From preliminary testing this performed well, and about the only weakness seems to be some false detections of loud plosives. But since this solution is based on the spectrum shape, different recording environments affecting the spectrum (and different resolutions, like the narrow spectrum of telephone transmissions) also influenced its performance. I also had to set two thresholds – one for general level and another for the mfcc, so while it could work well for one recording I had to fine tune the settings for the next, and that turned out too cumbersome for real time performance.

Finally, I turned to a very interesting approach based on measuring glottal activity as indicator of vowel presence (Yegnanarayana & Gangashetty, 2011). This elegant solution bypasses the short time Fourier transform altogether and instead looks at a characteristic feature of our voice: glottal pulses. These are like other any short impulses in that they contain all frequencies, including the zero frequency. By making a zero frequency resonator and looking at the small fluctuations at 0 Hz, it is possible to filter out almost all other sound and only have the glottal pulse train, ie the voiced parts of speech. I implemented this technique to detect glottal activity, and used that measure to gate the level of a formant region bandpassed (500-2500 Hz) loudness envelope. For realtime use, this proved more stable and robust than any of the other approaches, and paired with a peak picking process detecting onsets and endpoints rather than only peaks of syllable nuclei, this is the solution on which I have based my further development of real time speech analysis tools.